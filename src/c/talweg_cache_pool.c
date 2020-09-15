#include "talweg_cache.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    talweg_lba_t lba;
    uint8_t *data;
    uint32_t pin_count;
    bool is_dirty;
    bool is_valid;
    uint64_t access_seq;
} cache_entry_t;

struct talweg_cache_pool {
    talweg_cache_cfg_t cfg;
    cache_entry_t *entries;
    pthread_mutex_t lock;
    uint64_t access_clock;
    uint64_t hits;
    uint64_t misses;
};

int talweg_cache_pool_init(talweg_cache_pool_t **out_pool, const talweg_cache_cfg_t *cfg) {
    if (!out_pool || !cfg) return TALWEG_ERR_INVAL;

    talweg_cache_pool_t *p = (talweg_cache_pool_t *)calloc(1, sizeof(talweg_cache_pool_t));
    if (!p) return TALWEG_ERR_NOMEM;

    p->cfg = *cfg;
    if (p->cfg.capacity_pages == 0) p->cfg.capacity_pages = 64;
    if (p->cfg.page_size == 0) p->cfg.page_size = 4096;

    p->entries = (cache_entry_t *)calloc(p->cfg.capacity_pages, sizeof(cache_entry_t));
    if (!p->entries) {
        free(p);
        return TALWEG_ERR_NOMEM;
    }

    for (uint32_t i = 0; i < p->cfg.capacity_pages; i++) {
        p->entries[i].data = (uint8_t *)malloc(p->cfg.page_size);
        if (!p->entries[i].data) {
            for (uint32_t j = 0; j < i; j++) free(p->entries[j].data);
            free(p->entries);
            free(p);
            return TALWEG_ERR_NOMEM;
        }
        p->entries[i].is_valid = false;
        p->entries[i].is_dirty = false;
        p->entries[i].pin_count = 0;
    }

    pthread_mutex_init(&p->lock, NULL);
    *out_pool = p;
    return TALWEG_OK;
}

void talweg_cache_pool_destroy(talweg_cache_pool_t *pool) {
    if (pool) {
        pthread_mutex_lock(&pool->lock);
        if (pool->entries) {
            for (uint32_t i = 0; i < pool->cfg.capacity_pages; i++) {
                if (pool->entries[i].data) free(pool->entries[i].data);
            }
            free(pool->entries);
        }
        pthread_mutex_unlock(&pool->lock);
        pthread_mutex_destroy(&pool->lock);
        free(pool);
    }
}

int talweg_cache_pool_pin(talweg_cache_pool_t *pool, talweg_lba_t lba, void **out_buf) {
    if (!pool || !out_buf) return TALWEG_ERR_INVAL;

    pthread_mutex_lock(&pool->lock);

    /* Check for cache hit */
    for (uint32_t i = 0; i < pool->cfg.capacity_pages; i++) {
        if (pool->entries[i].is_valid && pool->entries[i].lba == lba) {
            pool->hits++;
            pool->entries[i].pin_count++;
            pool->entries[i].access_seq = ++pool->access_clock;
            *out_buf = pool->entries[i].data;
            pthread_mutex_unlock(&pool->lock);
            return TALWEG_OK;
        }
    }

    /* Miss: find available or oldest unpinned entry */
    pool->misses++;
    int victim = -1;
    uint64_t oldest = UINT64_MAX;

    for (uint32_t i = 0; i < pool->cfg.capacity_pages; i++) {
        if (!pool->entries[i].is_valid) {
            victim = (int)i;
            break;
        }
        if (pool->entries[i].pin_count == 0 && pool->entries[i].access_seq < oldest) {
            oldest = pool->entries[i].access_seq;
            victim = (int)i;
        }
    }

    if (victim < 0) {
        pthread_mutex_unlock(&pool->lock);
        *out_buf = NULL;
        return TALWEG_ERR_NOMEM;
    }

    cache_entry_t *e = &pool->entries[victim];
    e->lba = lba;
    e->is_valid = true;
    e->is_dirty = false;
    e->pin_count = 1;
    e->access_seq = ++pool->access_clock;
    *out_buf = e->data;

    pthread_mutex_unlock(&pool->lock);
    return TALWEG_OK;
}

int talweg_cache_pool_unpin(talweg_cache_pool_t *pool, talweg_lba_t lba, bool mark_dirty) {
    if (!pool) return TALWEG_ERR_INVAL;

    pthread_mutex_lock(&pool->lock);
    for (uint32_t i = 0; i < pool->cfg.capacity_pages; i++) {
        if (pool->entries[i].is_valid && pool->entries[i].lba == lba) {
            if (pool->entries[i].pin_count > 0) {
                pool->entries[i].pin_count--;
            }
            if (mark_dirty) {
                pool->entries[i].is_dirty = true;
            }
            pthread_mutex_unlock(&pool->lock);
            return TALWEG_OK;
        }
    }
    pthread_mutex_unlock(&pool->lock);
    return TALWEG_ERR_NOT_FOUND;
}

void talweg_cache_pool_invalidate(talweg_cache_pool_t *pool, talweg_lba_t lba) {
    if (!pool) return;
    pthread_mutex_lock(&pool->lock);
    for (uint32_t i = 0; i < pool->cfg.capacity_pages; i++) {
        if (pool->entries[i].is_valid && pool->entries[i].lba == lba) {
            pool->entries[i].is_valid = false;
            pool->entries[i].is_dirty = false;
            pool->entries[i].pin_count = 0;
            break;
        }
    }
    pthread_mutex_unlock(&pool->lock);
}

uint64_t talweg_cache_pool_hits(talweg_cache_pool_t *pool) {
    if (!pool) return 0;
    pthread_mutex_lock(&pool->lock);
    uint64_t h = pool->hits;
    pthread_mutex_unlock(&pool->lock);
    return h;
}

uint64_t talweg_cache_pool_misses(talweg_cache_pool_t *pool) {
    if (!pool) return 0;
    pthread_mutex_lock(&pool->lock);
    uint64_t m = pool->misses;
    pthread_mutex_unlock(&pool->lock);
    return m;
}
