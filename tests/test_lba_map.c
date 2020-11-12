#include "talweg_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

extern void *talweg_ats_lba_map_create(uint32_t capacity);
extern void talweg_ats_lba_map_destroy(void *tbl);
extern uint32_t talweg_ats_lba_map_lookup(void *tbl, uint32_t lba);
extern bool talweg_ats_lba_map_is_mapped(void *tbl, uint32_t lba);
extern uint32_t talweg_ats_lba_map_count_mapped(void *tbl);
extern uint32_t talweg_ats_lba_map_get_capacity(void *tbl);
extern uint32_t talweg_ats_lba_map_update(void *tbl, uint32_t lba, uint32_t new_ppn);
extern uint32_t talweg_ats_lba_map_unmap(void *tbl, uint32_t lba);

int main(void) {
    printf("=== Test: L2P Page Mapping Table ===\n");

    uint32_t cap = 1024;
    void *tbl = talweg_ats_lba_map_create(cap);
    assert(tbl != NULL);
    assert(talweg_ats_lba_map_get_capacity(tbl) == cap);
    assert(talweg_ats_lba_map_count_mapped(tbl) == 0);

    /* 1. All LBAs initially unmapped */
    for (uint32_t i = 0; i < cap; i++) {
        assert(talweg_ats_lba_map_lookup(tbl, i) == TALWEG_UNMAPPED_PPN);
        assert(talweg_ats_lba_map_is_mapped(tbl, i) == false);
    }

    /* 2. Map LBAs and verify count */
    for (uint32_t i = 0; i < 100; i++) {
        uint32_t old = talweg_ats_lba_map_update(tbl, i, i * 2);
        assert(old == TALWEG_UNMAPPED_PPN);
        assert(talweg_ats_lba_map_lookup(tbl, i) == i * 2);
        assert(talweg_ats_lba_map_is_mapped(tbl, i) == true);
    }
    assert(talweg_ats_lba_map_count_mapped(tbl) == 100);

    /* 3. Re-map LBA updates without increasing count */
    uint32_t old_ppn = talweg_ats_lba_map_update(tbl, 42, 9999);
    assert(old_ppn == 84);
    assert(talweg_ats_lba_map_lookup(tbl, 42) == 9999);
    assert(talweg_ats_lba_map_count_mapped(tbl) == 100);

    /* 4. Unmap LBA decreases count */
    old_ppn = talweg_ats_lba_map_unmap(tbl, 42);
    assert(old_ppn == 9999);
    assert(talweg_ats_lba_map_lookup(tbl, 42) == TALWEG_UNMAPPED_PPN);
    assert(talweg_ats_lba_map_is_mapped(tbl, 42) == false);
    assert(talweg_ats_lba_map_count_mapped(tbl) == 99);

    /* 5. Out of bounds handling */
    assert(talweg_ats_lba_map_lookup(tbl, cap + 10) == TALWEG_UNMAPPED_PPN);
    assert(talweg_ats_lba_map_update(tbl, cap + 10, 100) == TALWEG_INVALID_PPN);

    talweg_ats_lba_map_destroy(tbl);
    printf("PASS: All L2P mapping table tests passed.\n");
    return 0;
}
