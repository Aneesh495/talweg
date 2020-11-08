#include "talweg_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern uint32_t talweg_ats_ppn_to_block(uint32_t ppn, uint32_t ppb);
extern uint32_t talweg_ats_ppn_to_offset(uint32_t ppn, uint32_t ppb);
extern uint32_t talweg_ats_coords_to_ppn(uint32_t blk, uint32_t off, uint32_t ppb);
extern bool talweg_ats_lba_valid(int max_lba, int lba);
extern bool talweg_ats_ppn_valid(int total_pages, int ppn);
extern bool talweg_ats_block_valid(int total_blocks, int blk);

int main(void) {
    printf("=== Test: Flash Physical Geometry & Bounds ===\n");

    uint32_t ppb = 64;
    uint32_t total_blocks = 256;
    uint32_t total_pages = total_blocks * ppb;

    /* 1. Coordinate transformations */
    for (uint32_t blk = 0; blk < 10; blk++) {
        for (uint32_t off = 0; off < ppb; off++) {
            uint32_t ppn = talweg_ats_coords_to_ppn(blk, off, ppb);
            assert(ppn == blk * ppb + off);
            assert(talweg_ats_ppn_to_block(ppn, ppb) == blk);
            assert(talweg_ats_ppn_to_offset(ppn, ppb) == off);
        }
    }

    /* 2. Boundary predicates */
    assert(talweg_ats_lba_valid(1000, 0) == true);
    assert(talweg_ats_lba_valid(1000, 999) == true);
    assert(talweg_ats_lba_valid(1000, 1000) == false);
    assert(talweg_ats_lba_valid(1000, -1) == false);

    assert(talweg_ats_ppn_valid((int)total_pages, 0) == true);
    assert(talweg_ats_ppn_valid((int)total_pages, (int)total_pages - 1) == true);
    assert(talweg_ats_ppn_valid((int)total_pages, (int)total_pages) == false);
    assert(talweg_ats_ppn_valid((int)total_pages, -5) == false);

    assert(talweg_ats_block_valid((int)total_blocks, 0) == true);
    assert(talweg_ats_block_valid((int)total_blocks, (int)total_blocks - 1) == true);
    assert(talweg_ats_block_valid((int)total_blocks, (int)total_blocks) == false);

    printf("PASS: All geometry and boundary tests passed.\n");
    return 0;
}
