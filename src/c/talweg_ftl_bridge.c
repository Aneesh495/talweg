#include "talweg_ftl.h"
#include "talweg_crc32.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Forward declarations from ATS generated C symbols */
extern void *talweg_ats_ftl_create(uint32_t total, uint32_t ppb, uint32_t ps, uint32_t max_lba, int policy);
extern void talweg_ats_ftl_destroy(void *ftl);
extern int talweg_ats_ftl_read(void *ftl, uint32_t lba, void *buf);
extern int talweg_ats_ftl_write(void *ftl, uint32_t lba, const void *buf);
extern int talweg_ats_ftl_trim(void *ftl, uint32_t lba);
extern int talweg_ats_ftl_flush(void *ftl);
extern int talweg_ats_ftl_step_gc(void *ftl);
extern unsigned long long talweg_ats_ftl_get_user_writes(void *ftl);
extern unsigned long long talweg_ats_ftl_get_flash_writes(void *ftl);
extern unsigned long long talweg_ats_ftl_get_gc_cycles(void *ftl);
extern uint32_t talweg_ats_ftl_get_waf_mille(void *ftl);

extern uint32_t talweg_c_lba_map_update(void *p, uint32_t lba, uint32_t new_ppn);
extern void talweg_c_alloc_rebuild(void *p, const uint32_t *block_prog_counts, uint32_t active_blk, uint32_t active_off);

/* Internal fields from ATS context struct */
typedef struct {
    uint32_t total_blocks;
    uint32_t pages_per_block;
    uint32_t page_size;
    uint32_t max_lba;
    int gc_policy;

    void *map_tbl;
    void *allocator;
    void *wear_tracker;
    void *gc_engine;
    void *journal;
    void *bbt;
    void *cache;

    talweg_flash_dev_t *flash_dev;

    uint64_t user_writes;
    uint64_t flash_writes;
    uint64_t gc_cycles;
} talweg_ats_internal_t;

extern uint32_t talweg_c_alloc_get_free_count(void *p);
extern uint32_t talweg_c_wear_get_min_erase(void *p);
extern uint32_t talweg_c_wear_get_max_erase(void *p);
extern uint32_t talweg_c_wear_get_wear_delta(void *p);
extern uint32_t talweg_c_bbt_get_bad_count(void *p);
extern unsigned long long talweg_c_cache_get_hit_count(void *p);
extern unsigned long long talweg_c_cache_get_miss_count(void *p);
extern uint32_t talweg_c_lba_map_count_mapped(void *p);

struct talweg_ftl {
    talweg_ftl_config_t cfg;
    void *ats_ctx;
};

int talweg_ftl_init(talweg_ftl_t **out_ftl, const talweg_ftl_config_t *cfg) {
    if (!out_ftl || !cfg) return TALWEG_ERR_INVAL;

    talweg_ftl_t *ftl = (talweg_ftl_t *)calloc(1, sizeof(talweg_ftl_t));
    if (!ftl) return TALWEG_ERR_NOMEM;

    ftl->cfg = *cfg;
    ftl->ats_ctx = talweg_ats_ftl_create(cfg->geom.total_blocks,
                                         cfg->geom.pages_per_block,
                                         cfg->geom.page_size,
                                         cfg->geom.user_lba_count,
                                         cfg->gc_policy);
    if (!ftl->ats_ctx) {
        free(ftl);
        return TALWEG_ERR_NOMEM;
    }

    *out_ftl = ftl;
    return TALWEG_OK;
}

void talweg_ftl_free(talweg_ftl_t *ftl) {
    if (ftl) {
        if (ftl->ats_ctx) {
            talweg_ats_ftl_destroy(ftl->ats_ctx);
        }
        free(ftl);
    }
}

int talweg_ftl_attach_media(talweg_ftl_t *ftl, talweg_flash_dev_t *dev) {
    if (!ftl || !ftl->ats_ctx || !dev) return TALWEG_ERR_INVAL;
    talweg_ats_internal_t *ctx = (talweg_ats_internal_t *)ftl->ats_ctx;
    ctx->flash_dev = dev;

    if (!dev->raw_media) return TALWEG_OK;

    uint64_t *lba_seq = (uint64_t *)calloc(ctx->max_lba, sizeof(uint64_t));
    uint32_t *prog_counts = (uint32_t *)calloc(dev->geom.total_blocks, sizeof(uint32_t));
    if (!lba_seq || !prog_counts) {
        if (lba_seq) free(lba_seq);
        if (prog_counts) free(prog_counts);
        return TALWEG_ERR_NOMEM;
    }

    uint32_t ppb = dev->geom.pages_per_block;
    uint32_t total_blks = dev->geom.total_blocks;
    uint32_t active_candidate = TALWEG_INVALID_BLOCK;
    uint32_t active_offset = 0;
    bool has_any_writes = false;

    for (uint32_t blk = 0; blk < total_blks; blk++) {
        uint32_t count = 0;
        for (uint32_t p = 0; p < ppb; p++) {
            uint32_t ppn = blk * ppb + p;
            size_t offset = (size_t)ppn * dev->page_total_bytes;
            const uint8_t *src_oob_raw = dev->raw_media + offset + dev->geom.page_size;
            talweg_oob_t oob;
            memcpy(&oob, src_oob_raw, sizeof(talweg_oob_t));

            uint32_t expected_crc = talweg_crc32c_calc(&oob, offsetof(talweg_oob_t, crc_header));
            if (oob.crc_header == expected_crc && oob.lba < ctx->max_lba) {
                count++;
                has_any_writes = true;
                if (oob.seq >= lba_seq[oob.lba]) {
                    lba_seq[oob.lba] = oob.seq;
                    talweg_c_lba_map_update(ctx->map_tbl, oob.lba, ppn);
                    if (oob.seq > ctx->flash_writes) {
                        ctx->flash_writes = oob.seq;
                    }
                }
            }
        }
        prog_counts[blk] = count;
        if (count > 0 && count < ppb) {
            active_candidate = blk;
            active_offset = count;
        }
    }

    if (has_any_writes) {
        talweg_c_alloc_rebuild(ctx->allocator, prog_counts, active_candidate, active_offset);
    }

    free(lba_seq);
    free(prog_counts);
    return TALWEG_OK;
}

talweg_flash_dev_t *talweg_ftl_get_media(talweg_ftl_t *ftl) {
    if (!ftl || !ftl->ats_ctx) return NULL;
    talweg_ats_internal_t *ctx = (talweg_ats_internal_t *)ftl->ats_ctx;
    return ctx->flash_dev;
}

int talweg_ftl_read(talweg_ftl_t *ftl, talweg_lba_t lba, void *buf) {
    if (!ftl || !ftl->ats_ctx) return TALWEG_ERR_INVAL;
    return talweg_ats_ftl_read(ftl->ats_ctx, lba, buf);
}

int talweg_ftl_write(talweg_ftl_t *ftl, talweg_lba_t lba, const void *buf) {
    if (!ftl || !ftl->ats_ctx) return TALWEG_ERR_INVAL;
    return talweg_ats_ftl_write(ftl->ats_ctx, lba, (void *)buf);
}

int talweg_ftl_trim(talweg_ftl_t *ftl, talweg_lba_t lba) {
    if (!ftl || !ftl->ats_ctx) return TALWEG_ERR_INVAL;
    return talweg_ats_ftl_trim(ftl->ats_ctx, lba);
}

int talweg_ftl_flush(talweg_ftl_t *ftl) {
    if (!ftl || !ftl->ats_ctx) return TALWEG_ERR_INVAL;
    return talweg_ats_ftl_flush(ftl->ats_ctx);
}

int talweg_ftl_gc_step(talweg_ftl_t *ftl) {
    if (!ftl || !ftl->ats_ctx) return TALWEG_ERR_INVAL;
    return talweg_ats_ftl_step_gc(ftl->ats_ctx);
}

int talweg_ftl_get_stats(talweg_ftl_t *ftl, talweg_ftl_stats_t *out_stats) {
    if (!ftl || !ftl->ats_ctx || !out_stats) return TALWEG_ERR_INVAL;
    talweg_ats_internal_t *ctx = (talweg_ats_internal_t *)ftl->ats_ctx;

    out_stats->user_writes = talweg_ats_ftl_get_user_writes(ftl->ats_ctx);
    out_stats->flash_writes = talweg_ats_ftl_get_flash_writes(ftl->ats_ctx);
    out_stats->gc_cycles = talweg_ats_ftl_get_gc_cycles(ftl->ats_ctx);
    out_stats->waf_mille = talweg_ats_ftl_get_waf_mille(ftl->ats_ctx);

    out_stats->free_blocks = talweg_c_alloc_get_free_count(ctx->allocator);
    out_stats->min_erase = talweg_c_wear_get_min_erase(ctx->wear_tracker);
    out_stats->max_erase = talweg_c_wear_get_max_erase(ctx->wear_tracker);
    out_stats->wear_delta = talweg_c_wear_get_wear_delta(ctx->wear_tracker);
    out_stats->bad_blocks = talweg_c_bbt_get_bad_count(ctx->bbt);
    out_stats->cache_hits = talweg_c_cache_get_hit_count(ctx->cache);
    out_stats->cache_misses = talweg_c_cache_get_miss_count(ctx->cache);
    out_stats->mapped_lba_count = talweg_c_lba_map_count_mapped(ctx->map_tbl);

    return TALWEG_OK;
}
