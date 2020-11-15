#include "talweg_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern void *talweg_ats_wear_tracker_create(uint32_t total, uint32_t thresh);
extern void talweg_ats_wear_tracker_destroy(void *wt);
extern uint32_t talweg_ats_wear_tracker_record_erase(void *wt, uint32_t blk);
extern uint32_t talweg_ats_wear_tracker_get_erase_count(void *wt, uint32_t blk);
extern uint32_t talweg_ats_wear_tracker_get_min_erase(void *wt);
extern uint32_t talweg_ats_wear_tracker_get_max_erase(void *wt);
extern uint32_t talweg_ats_wear_tracker_get_wear_delta(void *wt);
extern bool talweg_ats_wear_tracker_needs_static_rebalance(void *wt);
extern uint32_t talweg_ats_wear_tracker_find_cold_candidate(void *wt);

int main(void) {
    printf("=== Test: Wear Leveling Dual-Pool Engine ===\n");

    uint32_t total = 64;
    uint32_t thresh = 10;
    void *wt = talweg_ats_wear_tracker_create(total, thresh);
    assert(wt != NULL);

    /* 1. Initial state: all erase counts 0, delta 0 */
    assert(talweg_ats_wear_tracker_get_min_erase(wt) == 0);
    assert(talweg_ats_wear_tracker_get_max_erase(wt) == 0);
    assert(talweg_ats_wear_tracker_get_wear_delta(wt) == 0);
    assert(talweg_ats_wear_tracker_needs_static_rebalance(wt) == false);

    /* 2. Record erases on a single hot block */
    for (uint32_t i = 1; i <= 9; i++) {
        uint32_t cnt = talweg_ats_wear_tracker_record_erase(wt, 5);
        assert(cnt == i);
        assert(talweg_ats_wear_tracker_get_max_erase(wt) == i);
        assert(talweg_ats_wear_tracker_get_min_erase(wt) == 0);
        assert(talweg_ats_wear_tracker_get_wear_delta(wt) == i);
        assert(talweg_ats_wear_tracker_needs_static_rebalance(wt) == false);
    }

    /* 3. Tenth erase reaches threshold: triggers rebalance invariant */
    talweg_ats_wear_tracker_record_erase(wt, 5);
    assert(talweg_ats_wear_tracker_get_wear_delta(wt) == 10);
    assert(talweg_ats_wear_tracker_needs_static_rebalance(wt) == true);

    /* 4. Find cold block candidate */
    uint32_t cold = talweg_ats_wear_tracker_find_cold_candidate(wt);
    assert(cold != 5);
    assert(talweg_ats_wear_tracker_get_erase_count(wt, cold) == 0);

    /* 5. Elevate erase counts on all other blocks */
    for (uint32_t b = 0; b < total; b++) {
        if (b != 5) {
            for (int k = 0; k < 8; k++) {
                talweg_ats_wear_tracker_record_erase(wt, b);
            }
        }
    }
    assert(talweg_ats_wear_tracker_get_min_erase(wt) == 8);
    assert(talweg_ats_wear_tracker_get_max_erase(wt) == 10);
    assert(talweg_ats_wear_tracker_get_wear_delta(wt) == 2);
    assert(talweg_ats_wear_tracker_needs_static_rebalance(wt) == false);

    talweg_ats_wear_tracker_destroy(wt);
    printf("PASS: All wear leveling engine tests passed.\n");
    return 0;
}
