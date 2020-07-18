(*
** Talweg: High-Assurance Flash Translation Layer
** Linear Block Allocator and Pool Management Implementation
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"
staload "./../SATS/talweg_alloc.sats"

%{^
#include "talweg_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t total_blocks;
    uint32_t pages_per_block;
    uint32_t free_count;
    uint32_t active_block;
    uint32_t active_offset;
    uint8_t *block_states;
    uint32_t *free_stack;
    uint32_t free_top;
} talweg_ats_allocator_t;
%}

extern fun talweg_c_alloc_create (total: uint, ppb: uint): ptr = "mac#talweg_c_alloc_create"
extern fun talweg_c_alloc_destroy (p: ptr): void = "mac#talweg_c_alloc_destroy"
extern fun talweg_c_alloc_get_total (p: ptr): uint = "mac#talweg_c_alloc_get_total"
extern fun talweg_c_alloc_get_free_count (p: ptr): uint = "mac#talweg_c_alloc_get_free_count"
extern fun talweg_c_alloc_get_active_block (p: ptr): uint = "mac#talweg_c_alloc_get_active_block"
extern fun talweg_c_alloc_get_active_offset (p: ptr): uint = "mac#talweg_c_alloc_get_active_offset"
extern fun talweg_c_alloc_next_page (p: ptr, out_ppn: &uint? >> uint): int = "mac#talweg_c_alloc_next_page"
extern fun talweg_c_alloc_acquire_free (p: ptr): uint = "mac#talweg_c_alloc_acquire_free"
extern fun talweg_c_alloc_release_erased (p: ptr, blk: uint): void = "mac#talweg_c_alloc_release_erased"
extern fun talweg_c_alloc_mark_bad (p: ptr, blk: uint): void = "mac#talweg_c_alloc_mark_bad"

assume block_allocator_vtype = ptr

implement block_alloc_create (total, ppb) = talweg_c_alloc_create (total, ppb)
implement block_alloc_destroy (alloc) = talweg_c_alloc_destroy (alloc)
implement block_alloc_get_total (alloc) = talweg_c_alloc_get_total (alloc)
implement block_alloc_get_free_count (alloc) = talweg_c_alloc_get_free_count (alloc)
implement block_alloc_get_active_block (alloc) = talweg_c_alloc_get_active_block (alloc)
implement block_alloc_get_active_offset (alloc) = talweg_c_alloc_get_active_offset (alloc)
implement block_alloc_next_page (alloc, out_ppn) = talweg_c_alloc_next_page (alloc, out_ppn)
implement block_alloc_acquire_free (alloc) = talweg_c_alloc_acquire_free (alloc)
implement block_alloc_release_erased (alloc, blk) = talweg_c_alloc_release_erased (alloc, blk)
implement block_alloc_mark_bad (alloc, blk) = talweg_c_alloc_mark_bad (alloc, blk)

%{
void *talweg_c_alloc_create(uint32_t total, uint32_t ppb) {
    if (total == 0 || ppb == 0) return NULL;
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)calloc(1, sizeof(talweg_ats_allocator_t));
    if (!a) return NULL;

    a->total_blocks = total;
    a->pages_per_block = ppb;
    a->block_states = (uint8_t *)calloc(total, sizeof(uint8_t));
    a->free_stack = (uint32_t *)malloc(total * sizeof(uint32_t));
    if (!a->block_states || !a->free_stack) {
        if (a->block_states) free(a->block_states);
        if (a->free_stack) free(a->free_stack);
        free(a);
        return NULL;
    }

    a->free_top = 0;
    for (uint32_t i = 0; i < total; i++) {
        a->block_states[i] = TALWEG_BLK_FREE;
        a->free_stack[a->free_top++] = i;
    }
    a->free_count = total;

    /* Pop the first block as the initial active block */
    if (a->free_top > 0) {
        a->active_block = a->free_stack[--a->free_top];
        a->free_count--;
        a->block_states[a->active_block] = TALWEG_BLK_OPEN;
        a->active_offset = 0;
    } else {
        a->active_block = TALWEG_INVALID_BLOCK;
        a->active_offset = 0;
    }

    return (void *)a;
}

void talweg_c_alloc_destroy(void *p) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    if (a) {
        if (a->block_states) free(a->block_states);
        if (a->free_stack) free(a->free_stack);
        free(a);
    }
}

uint32_t talweg_c_alloc_get_total(void *p) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    return a ? a->total_blocks : 0;
}

uint32_t talweg_c_alloc_get_free_count(void *p) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    return a ? a->free_count : 0;
}

uint32_t talweg_c_alloc_get_active_block(void *p) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    return a ? a->active_block : TALWEG_INVALID_BLOCK;
}

uint32_t talweg_c_alloc_get_active_offset(void *p) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    return a ? a->active_offset : 0;
}

int talweg_c_alloc_next_page(void *p, uint32_t *out_ppn) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    if (!a || !out_ppn) return TALWEG_ERR_INVAL;

    if (a->active_block == TALWEG_INVALID_BLOCK || a->active_offset >= a->pages_per_block) {
        if (a->active_block != TALWEG_INVALID_BLOCK) {
            a->block_states[a->active_block] = TALWEG_BLK_CLOSED;
        }
        if (a->free_top == 0) {
            *out_ppn = TALWEG_INVALID_PPN;
            return TALWEG_ERR_NO_FREE_BLOCKS;
        }
        a->active_block = a->free_stack[--a->free_top];
        a->free_count--;
        a->block_states[a->active_block] = TALWEG_BLK_OPEN;
        a->active_offset = 0;
    }

    *out_ppn = a->active_block * a->pages_per_block + a->active_offset;
    a->active_offset++;
    return TALWEG_OK;
}

uint32_t talweg_c_alloc_acquire_free(void *p) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    if (!a || a->free_top == 0) return TALWEG_INVALID_BLOCK;
    uint32_t blk = a->free_stack[--a->free_top];
    a->free_count--;
    a->block_states[blk] = TALWEG_BLK_OPEN;
    return blk;
}

void talweg_c_alloc_release_erased(void *p, uint32_t blk) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    if (!a || blk >= a->total_blocks) return;
    a->block_states[blk] = TALWEG_BLK_FREE;
    a->free_stack[a->free_top++] = blk;
    a->free_count++;
}

void talweg_c_alloc_mark_bad(void *p, uint32_t blk) {
    talweg_ats_allocator_t *a = (talweg_ats_allocator_t *)p;
    if (!a || blk >= a->total_blocks) return;
    a->block_states[blk] = TALWEG_BLK_BAD;
}
%}
