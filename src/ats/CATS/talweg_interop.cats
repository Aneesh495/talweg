#ifndef TALWEG_INTEROP_CATS
#define TALWEG_INTEROP_CATS

#include "talweg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t talweg_ats_ppn_to_block(uint32_t ppn, uint32_t ppb);
uint32_t talweg_ats_ppn_to_offset(uint32_t ppn, uint32_t ppb);
uint32_t talweg_ats_coords_to_ppn(uint32_t blk, uint32_t off, uint32_t ppb);
bool talweg_ats_lba_valid(int max_lba, int lba);
bool talweg_ats_ppn_valid(int total_pages, int ppn);
bool talweg_ats_block_valid(int total_blocks, int blk);

void *talweg_ats_lba_map_create(uint32_t capacity);
void talweg_ats_lba_map_destroy(void *tbl);
uint32_t talweg_ats_lba_map_lookup(void *tbl, uint32_t lba);
bool talweg_ats_lba_map_is_mapped(void *tbl, uint32_t lba);
uint32_t talweg_ats_lba_map_count_mapped(void *tbl);
uint32_t talweg_ats_lba_map_get_capacity(void *tbl);
uint32_t talweg_ats_lba_map_update(void *tbl, uint32_t lba, uint32_t new_ppn);
uint32_t talweg_ats_lba_map_unmap(void *tbl, uint32_t lba);

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_INTEROP_CATS */
