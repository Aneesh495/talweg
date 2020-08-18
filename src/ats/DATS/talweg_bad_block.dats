(*
** Talweg: High-Assurance Flash Translation Layer
** Bad Block Table and Defect Retirement Implementation
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"
staload "./../SATS/talweg_bad_block.sats"

%{^
#include "talweg_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t total_blocks;
    uint32_t reserve_blocks;
    uint32_t bad_count;
    uint32_t spares_used;
    uint8_t *bad_flags;
    uint32_t *remap_table;
    uint32_t *spare_pool;
} talweg_ats_bbt_t;
%}

extern fun talweg_c_bbt_create (total: uint, reserve: uint): ptr = "mac#talweg_c_bbt_create"
extern fun talweg_c_bbt_destroy (p: ptr): void = "mac#talweg_c_bbt_destroy"
extern fun talweg_c_bbt_mark_bad (p: ptr, blk: uint): int = "mac#talweg_c_bbt_mark_bad"
extern fun talweg_c_bbt_is_bad (p: ptr, blk: uint): bool = "mac#talweg_c_bbt_is_bad"
extern fun talweg_c_bbt_get_remap (p: ptr, blk: uint): uint = "mac#talweg_c_bbt_get_remap"
extern fun talweg_c_bbt_get_bad_count (p: ptr): uint = "mac#talweg_c_bbt_get_bad_count"
extern fun talweg_c_bbt_get_remaining_spares (p: ptr): uint = "mac#talweg_c_bbt_get_remaining_spares"

assume bbt_engine_vtype = ptr

implement bbt_engine_create (total, reserve) = talweg_c_bbt_create (total, reserve)
implement bbt_engine_destroy (bbt) = talweg_c_bbt_destroy (bbt)
implement bbt_mark_bad (bbt, blk) = talweg_c_bbt_mark_bad (bbt, blk)
implement bbt_is_bad (bbt, blk) = talweg_c_bbt_is_bad (bbt, blk)
implement bbt_get_remap (bbt, blk) = talweg_c_bbt_get_remap (bbt, blk)
implement bbt_get_bad_count (bbt) = talweg_c_bbt_get_bad_count (bbt)
implement bbt_get_remaining_spares (bbt) = talweg_c_bbt_get_remaining_spares (bbt)

%{
void *talweg_c_bbt_create(uint32_t total, uint32_t reserve) {
    if (total == 0) return NULL;
    talweg_ats_bbt_t *b = (talweg_ats_bbt_t *)calloc(1, sizeof(talweg_ats_bbt_t));
    if (!b) return NULL;

    b->total_blocks = total;
    b->reserve_blocks = (reserve > 0) ? reserve : 8;
    b->bad_flags = (uint8_t *)calloc(total, sizeof(uint8_t));
    b->remap_table = (uint32_t *)malloc(total * sizeof(uint32_t));
    b->spare_pool = (uint32_t *)malloc(b->reserve_blocks * sizeof(uint32_t));

    if (!b->bad_flags || !b->remap_table || !b->spare_pool) {
        if (b->bad_flags) free(b->bad_flags);
        if (b->remap_table) free(b->remap_table);
        if (b->spare_pool) free(b->spare_pool);
        free(b);
        return NULL;
    }

    for (uint32_t i = 0; i < total; i++) {
        b->remap_table[i] = i; /* 1:1 identity initially */
    }

    /* Assign spare blocks from the tail of the block array */
    for (uint32_t i = 0; i < b->reserve_blocks; i++) {
        b->spare_pool[i] = (total > b->reserve_blocks) ? (total - b->reserve_blocks + i) : i;
    }
    b->spares_used = 0;
    b->bad_count = 0;

    return (void *)b;
}

void talweg_c_bbt_destroy(void *p) {
    talweg_ats_bbt_t *b = (talweg_ats_bbt_t *)p;
    if (b) {
        if (b->bad_flags) free(b->bad_flags);
        if (b->remap_table) free(b->remap_table);
        if (b->spare_pool) free(b->spare_pool);
        free(b);
    }
}

int talweg_c_bbt_mark_bad(void *p, uint32_t blk) {
    talweg_ats_bbt_t *b = (talweg_ats_bbt_t *)p;
    if (!b || blk >= b->total_blocks) return TALWEG_ERR_INVAL;

    if (b->bad_flags[blk]) {
        return TALWEG_OK; /* Already marked */
    }

    b->bad_flags[blk] = 1;
    b->bad_count++;

    /* Allocate spare block if available */
    if (b->spares_used < b->reserve_blocks) {
        uint32_t spare = b->spare_pool[b->spares_used++];
        b->remap_table[blk] = spare;
        return TALWEG_OK;
    }

    b->remap_table[blk] = TALWEG_INVALID_BLOCK;
    return TALWEG_ERR_NO_FREE_BLOCKS;
}

bool talweg_c_bbt_is_bad(void *p, uint32_t blk) {
    talweg_ats_bbt_t *b = (talweg_ats_bbt_t *)p;
    if (!b || blk >= b->total_blocks) return true;
    return b->bad_flags[blk] != 0;
}

uint32_t talweg_c_bbt_get_remap(void *p, uint32_t blk) {
    talweg_ats_bbt_t *b = (talweg_ats_bbt_t *)p;
    if (!b || blk >= b->total_blocks) return TALWEG_INVALID_BLOCK;
    return b->remap_table[blk];
}

uint32_t talweg_c_bbt_get_bad_count(void *p) {
    talweg_ats_bbt_t *b = (talweg_ats_bbt_t *)p;
    return b ? b->bad_count : 0;
}

uint32_t talweg_c_bbt_get_remaining_spares(void *p) {
    talweg_ats_bbt_t *b = (talweg_ats_bbt_t *)p;
    if (!b) return 0;
    return (b->reserve_blocks >= b->spares_used) ? (b->reserve_blocks - b->spares_used) : 0;
}
%}
