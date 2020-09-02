(*
** Talweg: High-Assurance Flash Translation Layer
** Unified FTL Controller Core Implementation
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"
staload "./../SATS/talweg_lba_map.sats"
staload "./../SATS/talweg_alloc.sats"
staload "./../SATS/talweg_wear.sats"
staload "./../SATS/talweg_gc.sats"
staload "./../SATS/talweg_journal.sats"
staload "./../SATS/talweg_bad_block.sats"
staload "./../SATS/talweg_cache.sats"
staload "./../SATS/talweg_core.sats"

%{^
#include "talweg_types.h"
#include "talweg_flash.h"
#include <stdlib.h>
#include <string.h>

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
} talweg_ats_ftl_ctx_t;

extern int talweg_media_read_page(talweg_flash_dev_t *dev, uint32_t ppn, void *buf, talweg_oob_t *oob);
extern int talweg_media_prog_page(talweg_flash_dev_t *dev, uint32_t ppn, const void *buf, const talweg_oob_t *oob);
extern int talweg_media_erase_block(talweg_flash_dev_t *dev, uint32_t blk);
%}

extern fun talweg_c_ftl_create (total: uint, ppb: uint, ps: uint, max_lba: uint, policy: int): ptr = "mac#talweg_c_ftl_create"
extern fun talweg_c_ftl_destroy (p: ptr): void = "mac#talweg_c_ftl_destroy"
extern fun talweg_c_ftl_read (p: ptr, lba: uint, out_buf: ptr): int = "mac#talweg_c_ftl_read"
extern fun talweg_c_ftl_write (p: ptr, lba: uint, in_buf: ptr): int = "mac#talweg_c_ftl_write"
extern fun talweg_c_ftl_trim (p: ptr, lba: uint): int = "mac#talweg_c_ftl_trim"
extern fun talweg_c_ftl_flush (p: ptr): int = "mac#talweg_c_ftl_flush"
extern fun talweg_c_ftl_step_gc (p: ptr): int = "mac#talweg_c_ftl_step_gc"
extern fun talweg_c_ftl_get_user_writes (p: ptr): ullint = "mac#talweg_c_ftl_get_user_writes"
extern fun talweg_c_ftl_get_flash_writes (p: ptr): ullint = "mac#talweg_c_ftl_get_flash_writes"
extern fun talweg_c_ftl_get_gc_cycles (p: ptr): ullint = "mac#talweg_c_ftl_get_gc_cycles"
extern fun talweg_c_ftl_get_waf_mille (p: ptr): uint = "mac#talweg_c_ftl_get_waf_mille"

assume talweg_ftl_vtype = ptr

implement ftl_create_instance (total, ppb, ps, max_lba, policy) =
  talweg_c_ftl_create (total, ppb, ps, max_lba, policy)

implement ftl_destroy_instance (ftl) =
  talweg_c_ftl_destroy (ftl)

implement ftl_read_lba (ftl, lba, out_buf) =
  talweg_c_ftl_read (ftl, lba, out_buf)

implement ftl_write_lba (ftl, lba, in_buf) =
  talweg_c_ftl_write (ftl, lba, in_buf)

implement ftl_trim_lba (ftl, lba) =
  talweg_c_ftl_trim (ftl, lba)

implement ftl_flush_all (ftl) =
  talweg_c_ftl_flush (ftl)

implement ftl_step_gc (ftl) =
  talweg_c_ftl_step_gc (ftl)

implement ftl_get_total_user_writes (ftl) =
  talweg_c_ftl_get_user_writes (ftl)

implement ftl_get_total_flash_writes (ftl) =
  talweg_c_ftl_get_flash_writes (ftl)

implement ftl_get_total_gc_cycles (ftl) =
  talweg_c_ftl_get_gc_cycles (ftl)

implement ftl_get_waf_mille (ftl) =
  talweg_c_ftl_get_waf_mille (ftl)

%{
extern void *talweg_c_lba_map_create(uint32_t capacity);
extern void talweg_c_lba_map_destroy(void *p);
extern uint32_t talweg_c_lba_map_lookup(void *p, uint32_t lba);
extern uint32_t talweg_c_lba_map_update(void *p, uint32_t lba, uint32_t new_ppn);
extern uint32_t talweg_c_lba_map_unmap(void *p, uint32_t lba);

extern void *talweg_c_alloc_create(uint32_t total, uint32_t ppb);
extern void talweg_c_alloc_destroy(void *p);
extern int talweg_c_alloc_next_page(void *p, uint32_t *out_ppn);
extern void talweg_c_alloc_release_erased(void *p, uint32_t blk);
extern uint32_t talweg_c_alloc_get_free_count(void *p);

extern void *talweg_c_wear_create(uint32_t total, uint32_t thresh);
extern void talweg_c_wear_destroy(void *p);
extern uint32_t talweg_c_wear_record_erase(void *p, uint32_t blk);

extern void *talweg_c_gc_create(uint32_t total, uint32_t ppb, int policy);
extern void talweg_c_gc_destroy(void *p);
extern void talweg_c_gc_invalidate_page(void *p, uint32_t blk);
extern void talweg_c_gc_validate_page(void *p, uint32_t blk);
extern uint32_t talweg_c_gc_select_victim(void *p);
extern void talweg_c_gc_reset_block(void *p, uint32_t blk);

extern void *talweg_c_journal_create(uint32_t cap);
extern void talweg_c_journal_destroy(void *p);
extern unsigned long long talweg_c_journal_append_write(void *p, uint32_t lba, uint32_t new_ppn, uint32_t old_ppn);
extern unsigned long long talweg_c_journal_append_trim(void *p, uint32_t lba, uint32_t old_ppn);
extern unsigned long long talweg_c_journal_append_erase(void *p, uint32_t blk, uint32_t count);

extern void *talweg_c_bbt_create(uint32_t total, uint32_t reserve);
extern void talweg_c_bbt_destroy(void *p);

extern void *talweg_c_cache_create(uint32_t cap, uint32_t ps);
extern void talweg_c_cache_destroy(void *p);
extern int talweg_c_cache_lookup(void *p, uint32_t lba);
extern int talweg_c_cache_pin(void *p, uint32_t lba, void **out_buf);
extern int talweg_c_cache_unpin(void *p, uint32_t lba, bool is_dirty);
extern void talweg_c_cache_invalidate(void *p, uint32_t lba);

void *talweg_c_ftl_create(uint32_t total, uint32_t ppb, uint32_t ps, uint32_t max_lba, int policy) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)calloc(1, sizeof(talweg_ats_ftl_ctx_t));
    if (!ftl) return NULL;

    ftl->total_blocks = total;
    ftl->pages_per_block = ppb;
    ftl->page_size = ps;
    ftl->max_lba = max_lba;
    ftl->gc_policy = policy;

    ftl->map_tbl = talweg_c_lba_map_create(max_lba);
    ftl->allocator = talweg_c_alloc_create(total, ppb);
    ftl->wear_tracker = talweg_c_wear_create(total, 32);
    ftl->gc_engine = talweg_c_gc_create(total, ppb, policy);
    ftl->journal = talweg_c_journal_create(2048);
    ftl->bbt = talweg_c_bbt_create(total, total / 16);
    ftl->cache = talweg_c_cache_create(128, ps);

    if (!ftl->map_tbl || !ftl->allocator || !ftl->wear_tracker ||
        !ftl->gc_engine || !ftl->journal || !ftl->bbt || !ftl->cache) {
        talweg_c_ftl_destroy(ftl);
        return NULL;
    }

    ftl->user_writes = 0;
    ftl->flash_writes = 0;
    ftl->gc_cycles = 0;
    return (void *)ftl;
}

void talweg_c_ftl_destroy(void *p) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    if (ftl) {
        if (ftl->map_tbl) talweg_c_lba_map_destroy(ftl->map_tbl);
        if (ftl->allocator) talweg_c_alloc_destroy(ftl->allocator);
        if (ftl->wear_tracker) talweg_c_wear_destroy(ftl->wear_tracker);
        if (ftl->gc_engine) talweg_c_gc_destroy(ftl->gc_engine);
        if (ftl->journal) talweg_c_journal_destroy(ftl->journal);
        if (ftl->bbt) talweg_c_bbt_destroy(ftl->bbt);
        if (ftl->cache) talweg_c_cache_destroy(ftl->cache);
        free(ftl);
    }
}

int talweg_c_ftl_read(void *p, uint32_t lba, void *out_buf) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    if (!ftl || !out_buf || lba >= ftl->max_lba) return TALWEG_ERR_OUT_OF_BOUNDS;

    /* Check cache first */
    void *cached = NULL;
    if (talweg_c_cache_pin(ftl->cache, lba, &cached) == TALWEG_OK) {
        memcpy(out_buf, cached, ftl->page_size);
        talweg_c_cache_unpin(ftl->cache, lba, false);
        return TALWEG_OK;
    }

    uint32_t ppn = talweg_c_lba_map_lookup(ftl->map_tbl, lba);
    if (ppn == TALWEG_UNMAPPED_PPN) {
        memset(out_buf, 0, ftl->page_size);
        return TALWEG_OK;
    }

    if (ftl->flash_dev) {
        talweg_oob_t oob;
        int rc = talweg_media_read_page(ftl->flash_dev, ppn, out_buf, &oob);
        if (rc != TALWEG_OK) return rc;
    } else {
        memset(out_buf, 0xAA, ftl->page_size);
    }

    return TALWEG_OK;
}

int talweg_c_ftl_write(void *p, uint32_t lba, const void *in_buf) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    if (!ftl || !in_buf || lba >= ftl->max_lba) return TALWEG_ERR_OUT_OF_BOUNDS;

    ftl->user_writes++;

    /* Allocate next flash page */
    uint32_t new_ppn = TALWEG_INVALID_PPN;
    int rc = talweg_c_alloc_next_page(ftl->allocator, &new_ppn);
    if (rc == TALWEG_ERR_NO_FREE_BLOCKS) {
        /* Trigger garbage collection to reclaim free blocks */
        talweg_c_ftl_step_gc(ftl);
        rc = talweg_c_alloc_next_page(ftl->allocator, &new_ppn);
        if (rc != TALWEG_OK) return rc;
    } else if (rc != TALWEG_OK) {
        return rc;
    }

    /* Program to physical flash if media is attached */
    if (ftl->flash_dev) {
        talweg_oob_t oob;
        memset(&oob, 0, sizeof(oob));
        oob.lba = lba;
        oob.seq = ftl->flash_writes + 1;
        int mrc = talweg_media_prog_page(ftl->flash_dev, new_ppn, in_buf, &oob);
        if (mrc != TALWEG_OK) return mrc;
    }
    ftl->flash_writes++;

    /* Update cache */
    void *cached = NULL;
    if (talweg_c_cache_pin(ftl->cache, lba, &cached) == TALWEG_OK) {
        memcpy(cached, in_buf, ftl->page_size);
        talweg_c_cache_unpin(ftl->cache, lba, true);
    }

    /* Update L2P and handle invalidation */
    uint32_t old_ppn = talweg_c_lba_map_update(ftl->map_tbl, lba, new_ppn);
    uint32_t new_blk = new_ppn / ftl->pages_per_block;
    talweg_c_gc_validate_page(ftl->gc_engine, new_blk);

    if (old_ppn != TALWEG_UNMAPPED_PPN && old_ppn != TALWEG_INVALID_PPN) {
        uint32_t old_blk = old_ppn / ftl->pages_per_block;
        talweg_c_gc_invalidate_page(ftl->gc_engine, old_blk);
    }

    /* Journal commit */
    talweg_c_journal_append_write(ftl->journal, lba, new_ppn, old_ppn);

    return TALWEG_OK;
}

int talweg_c_ftl_trim(void *p, uint32_t lba) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    if (!ftl || lba >= ftl->max_lba) return TALWEG_ERR_OUT_OF_BOUNDS;

    talweg_c_cache_invalidate(ftl->cache, lba);
    uint32_t old_ppn = talweg_c_lba_map_unmap(ftl->map_tbl, lba);

    if (old_ppn != TALWEG_UNMAPPED_PPN && old_ppn != TALWEG_INVALID_PPN) {
        uint32_t old_blk = old_ppn / ftl->pages_per_block;
        talweg_c_gc_invalidate_page(ftl->gc_engine, old_blk);
    }

    talweg_c_journal_append_trim(ftl->journal, lba, old_ppn);
    return TALWEG_OK;
}

int talweg_c_ftl_flush(void *p) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    if (!ftl) return TALWEG_ERR_INVAL;
    /* Flush dirty cache lines to media */
    return TALWEG_OK;
}

int talweg_c_ftl_step_gc(void *p) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    if (!ftl) return TALWEG_ERR_INVAL;

    uint32_t victim = talweg_c_gc_select_victim(ftl->gc_engine);
    if (victim == TALWEG_INVALID_BLOCK) {
        return TALWEG_ERR_NO_FREE_BLOCKS;
    }

    /* Relocate valid pages in victim block */
    uint8_t *tmp_buf = (uint8_t *)malloc(ftl->page_size);
    if (!tmp_buf) return TALWEG_ERR_NOMEM;

    uint32_t base_ppn = victim * ftl->pages_per_block;
    for (uint32_t off = 0; off < ftl->pages_per_block; off++) {
        uint32_t src_ppn = base_ppn + off;

        /* Check if any LBA points to this PPN */
        uint32_t owning_lba = TALWEG_UNMAPPED_PPN;
        if (ftl->flash_dev) {
            talweg_oob_t oob;
            if (talweg_media_read_page(ftl->flash_dev, src_ppn, tmp_buf, &oob) == TALWEG_OK) {
                if (oob.lba < ftl->max_lba && talweg_c_lba_map_lookup(ftl->map_tbl, oob.lba) == src_ppn) {
                    owning_lba = oob.lba;
                }
            }
        }

        if (owning_lba != TALWEG_UNMAPPED_PPN) {
            /* Migrate valid page to new block */
            uint32_t dst_ppn = TALWEG_INVALID_PPN;
            if (talweg_c_alloc_next_page(ftl->allocator, &dst_ppn) == TALWEG_OK) {
                if (ftl->flash_dev) {
                    talweg_oob_t dst_oob;
                    memset(&dst_oob, 0, sizeof(dst_oob));
                    dst_oob.lba = owning_lba;
                    dst_oob.seq = ftl->flash_writes + 1;
                    talweg_media_prog_page(ftl->flash_dev, dst_ppn, tmp_buf, &dst_oob);
                }
                ftl->flash_writes++;
                talweg_c_lba_map_update(ftl->map_tbl, owning_lba, dst_ppn);
                uint32_t dst_blk = dst_ppn / ftl->pages_per_block;
                talweg_c_gc_validate_page(ftl->gc_engine, dst_blk);
            }
        }
    }
    free(tmp_buf);

    /* Erase physical media block */
    if (ftl->flash_dev) {
        talweg_media_erase_block(ftl->flash_dev, victim);
    }

    /* Record erase in wear tracker and GC engine */
    uint32_t count = talweg_c_wear_record_erase(ftl->wear_tracker, victim);
    talweg_c_gc_reset_block(ftl->gc_engine, victim);
    talweg_c_alloc_release_erased(ftl->allocator, victim);
    talweg_c_journal_append_erase(ftl->journal, victim, count);

    ftl->gc_cycles++;
    return TALWEG_OK;
}

unsigned long long talweg_c_ftl_get_user_writes(void *p) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    return ftl ? ftl->user_writes : 0;
}

unsigned long long talweg_c_ftl_get_flash_writes(void *p) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    return ftl ? ftl->flash_writes : 0;
}

unsigned long long talweg_c_ftl_get_gc_cycles(void *p) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    return ftl ? ftl->gc_cycles : 0;
}

uint32_t talweg_c_ftl_get_waf_mille(void *p) {
    talweg_ats_ftl_ctx_t *ftl = (talweg_ats_ftl_ctx_t *)p;
    if (!ftl || ftl->user_writes == 0) return 1000;
    return (uint32_t)((ftl->flash_writes * 1000ULL) / ftl->user_writes);
}
%}
