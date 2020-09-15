#ifndef TALWEG_CACHE_H
#define TALWEG_CACHE_H

#include "talweg_types.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t capacity_pages;
    uint32_t page_size;
} talweg_cache_cfg_t;

typedef struct talweg_cache_pool talweg_cache_pool_t;

int talweg_cache_pool_init(talweg_cache_pool_t **out_pool, const talweg_cache_cfg_t *cfg);
void talweg_cache_pool_destroy(talweg_cache_pool_t *pool);

int talweg_cache_pool_pin(talweg_cache_pool_t *pool, talweg_lba_t lba, void **out_buf);
int talweg_cache_pool_unpin(talweg_cache_pool_t *pool, talweg_lba_t lba, bool mark_dirty);
int talweg_cache_pool_evict(talweg_cache_pool_t *pool, talweg_lba_t *out_lba, void **out_buf, bool *out_is_dirty);
void talweg_cache_pool_invalidate(talweg_cache_pool_t *pool, talweg_lba_t lba);

uint64_t talweg_cache_pool_hits(talweg_cache_pool_t *pool);
uint64_t talweg_cache_pool_misses(talweg_cache_pool_t *pool);

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_CACHE_H */
