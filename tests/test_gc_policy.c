#include "talweg_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern void *talweg_ats_gc_engine_create(uint32_t total, uint32_t ppb, int policy);
extern void talweg_ats_gc_engine_destroy(void *gc);
extern void talweg_ats_gc_engine_invalidate_page(void *gc, uint32_t blk);
extern void talweg_ats_gc_engine_validate_page(void *gc, uint32_t blk);
extern uint32_t talweg_ats_gc_engine_get_invalid_count(void *gc, uint32_t blk);
extern uint32_t talweg_ats_gc_engine_get_valid_count(void *gc, uint32_t blk);
extern uint32_t talweg_ats_gc_engine_select_victim(void *gc);
extern void talweg_ats_gc_engine_reset_block(void *gc, uint32_t blk);
extern void talweg_ats_gc_engine_set_policy(void *gc, int policy);

int main(void) {
    printf("=== Test: Garbage Collection Victim Selection ===\n");

    uint32_t total = 32;
    uint32_t ppb = 64;

    /* 1. Greedy policy test */
    void *gc = talweg_ats_gc_engine_create(total, ppb, TALWEG_GC_GREEDY);
    assert(gc != NULL);

    /* Fill block 3 with 20 invalid pages */
    for (int i = 0; i < 20; i++) {
        talweg_ats_gc_engine_invalidate_page(gc, 3);
    }
    assert(talweg_ats_gc_engine_get_invalid_count(gc, 3) == 20);

    /* Fill block 7 with 45 invalid pages */
    for (int i = 0; i < 45; i++) {
        talweg_ats_gc_engine_invalidate_page(gc, 7);
    }
    assert(talweg_ats_gc_engine_get_invalid_count(gc, 7) == 45);

    /* Fill block 10 with 10 invalid pages */
    for (int i = 0; i < 10; i++) {
        talweg_ats_gc_engine_invalidate_page(gc, 10);
    }

    /* Greedy must select block 7 */
    uint32_t victim = talweg_ats_gc_engine_select_victim(gc);
    assert(victim == 7);

    /* Reset block 7 and re-check */
    talweg_ats_gc_engine_reset_block(gc, 7);
    assert(talweg_ats_gc_engine_get_invalid_count(gc, 7) == 0);
    victim = talweg_ats_gc_engine_select_victim(gc);
    assert(victim == 3);

    /* 2. Switch to Cost-Benefit policy */
    talweg_ats_gc_engine_set_policy(gc, TALWEG_GC_COST_BENEFIT);
    victim = talweg_ats_gc_engine_select_victim(gc);
    assert(victim != TALWEG_INVALID_BLOCK);

    talweg_ats_gc_engine_destroy(gc);
    printf("PASS: All garbage collection policy tests passed.\n");
    return 0;
}
