#include "talweg_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(void) {
    printf("=== Test: Page Buffer Cache Pool Pinning & LRU ===\n");

    talweg_cache_cfg_t cfg;
    cfg.capacity_pages = 4;
    cfg.page_size = 4096;

    talweg_cache_pool_t *pool = NULL;
    assert(talweg_cache_pool_init(&pool, &cfg) == TALWEG_OK);

    void *buf1 = NULL;
    void *buf2 = NULL;
    void *buf3 = NULL;
    void *buf4 = NULL;
    void *buf5 = NULL;

    /* 1. Pin 4 distinct LBAs (fills entire capacity) */
    assert(talweg_cache_pool_pin(pool, 10, &buf1) == TALWEG_OK);
    assert(talweg_cache_pool_pin(pool, 20, &buf2) == TALWEG_OK);
    assert(talweg_cache_pool_pin(pool, 30, &buf3) == TALWEG_OK);
    assert(talweg_cache_pool_pin(pool, 40, &buf4) == TALWEG_OK);
    assert(talweg_cache_pool_misses(pool) == 4);

    /* 2. Fifth pin must fail because all frames are currently pinned */
    assert(talweg_cache_pool_pin(pool, 50, &buf5) == TALWEG_ERR_NOMEM);

    /* 3. Unpin frame 10 */
    assert(talweg_cache_pool_unpin(pool, 10, false) == TALWEG_OK);

    /* 4. Fifth pin succeeds by evicting unpinned frame 10 */
    assert(talweg_cache_pool_pin(pool, 50, &buf5) == TALWEG_OK);
    assert(buf5 != NULL);

    /* 5. Accessing existing pinned frame 20 produces a hit */
    void *buf2_again = NULL;
    assert(talweg_cache_pool_pin(pool, 20, &buf2_again) == TALWEG_OK);
    assert(buf2 == buf2_again);
    assert(talweg_cache_pool_hits(pool) == 1);

    /* Cleanup remaining pins */
    talweg_cache_pool_unpin(pool, 20, false);
    talweg_cache_pool_unpin(pool, 20, false);
    talweg_cache_pool_unpin(pool, 30, false);
    talweg_cache_pool_unpin(pool, 40, false);
    talweg_cache_pool_unpin(pool, 50, false);

    talweg_cache_pool_destroy(pool);
    printf("PASS: All cache pool pinning and safety tests passed.\n");
    return 0;
}
