#ifndef TALWEG_FTL_H
#define TALWEG_FTL_H

#include "talweg_types.h"
#include "talweg_flash.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct talweg_ftl talweg_ftl_t;

typedef struct {
    talweg_geometry_t geom;
    talweg_gc_policy_t gc_policy;
    uint32_t wear_delta_threshold;
    uint32_t cache_pages;
    uint32_t journal_capacity;
} talweg_ftl_config_t;

typedef struct {
    uint64_t user_writes;
    uint64_t flash_writes;
    uint64_t gc_cycles;
    uint32_t waf_mille;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint32_t bad_blocks;
    uint32_t free_blocks;
    uint32_t min_erase;
    uint32_t max_erase;
    uint32_t wear_delta;
    uint32_t mapped_lba_count;
} talweg_ftl_stats_t;

int talweg_ftl_init(talweg_ftl_t **out_ftl, const talweg_ftl_config_t *cfg);
void talweg_ftl_free(talweg_ftl_t *ftl);

int talweg_ftl_attach_media(talweg_ftl_t *ftl, talweg_flash_dev_t *dev);
talweg_flash_dev_t *talweg_ftl_get_media(talweg_ftl_t *ftl);

int talweg_ftl_read(talweg_ftl_t *ftl, talweg_lba_t lba, void *buf);
int talweg_ftl_write(talweg_ftl_t *ftl, talweg_lba_t lba, const void *buf);
int talweg_ftl_trim(talweg_ftl_t *ftl, talweg_lba_t lba);
int talweg_ftl_flush(talweg_ftl_t *ftl);
int talweg_ftl_gc_step(talweg_ftl_t *ftl);

int talweg_ftl_get_stats(talweg_ftl_t *ftl, talweg_ftl_stats_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_FTL_H */
