#include "talweg_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern void *talweg_ats_bbt_engine_create(uint32_t total, uint32_t reserve);
extern void talweg_ats_bbt_engine_destroy(void *bbt);
extern int talweg_ats_bbt_mark_bad(void *bbt, uint32_t blk);
extern bool talweg_ats_bbt_is_bad(void *bbt, uint32_t blk);
extern uint32_t talweg_ats_bbt_get_remap(void *bbt, uint32_t blk);
extern uint32_t talweg_ats_bbt_get_bad_count(void *bbt);
extern uint32_t talweg_ats_bbt_get_remaining_spares(void *bbt);

int main(void) {
    printf("=== Test: Bad Block Table & Spare Pool Remapping ===\n");

    uint32_t total = 64;
    uint32_t reserve = 4;
    void *bbt = talweg_ats_bbt_engine_create(total, reserve);
    assert(bbt != NULL);

    assert(talweg_ats_bbt_get_bad_count(bbt) == 0);
    assert(talweg_ats_bbt_get_remaining_spares(bbt) == 4);

    /* 1. Normal blocks are not bad and map to themselves */
    for (uint32_t i = 0; i < total; i++) {
        assert(!talweg_ats_bbt_is_bad(bbt, i));
        assert(talweg_ats_bbt_get_remap(bbt, i) == i);
    }

    /* 2. Mark block 7 bad: consumes 1 spare */
    assert(talweg_ats_bbt_mark_bad(bbt, 7) == TALWEG_OK);
    assert(talweg_ats_bbt_is_bad(bbt, 7));
    assert(talweg_ats_bbt_get_bad_count(bbt) == 1);
    assert(talweg_ats_bbt_get_remaining_spares(bbt) == 3);
    uint32_t spare1 = talweg_ats_bbt_get_remap(bbt, 7);
    assert(spare1 != 7);

    /* 3. Consume remaining 3 spares */
    assert(talweg_ats_bbt_mark_bad(bbt, 12) == TALWEG_OK);
    assert(talweg_ats_bbt_mark_bad(bbt, 20) == TALWEG_OK);
    assert(talweg_ats_bbt_mark_bad(bbt, 33) == TALWEG_OK);
    assert(talweg_ats_bbt_get_remaining_spares(bbt) == 0);
    assert(talweg_ats_bbt_get_bad_count(bbt) == 4);

    /* 4. Fifth bad block fails to get spare */
    assert(talweg_ats_bbt_mark_bad(bbt, 45) == TALWEG_ERR_NO_FREE_BLOCKS);
    assert(talweg_ats_bbt_is_bad(bbt, 45));
    assert(talweg_ats_bbt_get_remap(bbt, 45) == TALWEG_INVALID_BLOCK);

    talweg_ats_bbt_engine_destroy(bbt);
    printf("PASS: All bad block retirement tests passed.\n");
    return 0;
}
