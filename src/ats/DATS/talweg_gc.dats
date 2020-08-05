(*
** Talweg: High-Assurance Flash Translation Layer
** Garbage Collection Policy Engine Implementation
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"
staload "./../SATS/talweg_gc.sats"

%{^
#include "talweg_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t total_blocks;
    uint32_t pages_per_block;
    int policy;
    uint32_t *valid_counts;
    uint32_t *invalid_counts;
    uint64_t *last_mod_time;
    uint64_t logical_clock;
} talweg_ats_gc_t;
%}

extern fun talweg_c_gc_create (total: uint, ppb: uint, policy: int): ptr = "mac#talweg_c_gc_create"
extern fun talweg_c_gc_destroy (p: ptr): void = "mac#talweg_c_gc_destroy"
extern fun talweg_c_gc_invalidate_page (p: ptr, blk: uint): void = "mac#talweg_c_gc_invalidate_page"
extern fun talweg_c_gc_validate_page (p: ptr, blk: uint): void = "mac#talweg_c_gc_validate_page"
extern fun talweg_c_gc_get_invalid_count (p: ptr, blk: uint): uint = "mac#talweg_c_gc_get_invalid_count"
extern fun talweg_c_gc_get_valid_count (p: ptr, blk: uint): uint = "mac#talweg_c_gc_get_valid_count"
extern fun talweg_c_gc_select_victim (p: ptr): uint = "mac#talweg_c_gc_select_victim"
extern fun talweg_c_gc_reset_block (p: ptr, blk: uint): void = "mac#talweg_c_gc_reset_block"
extern fun talweg_c_gc_set_policy (p: ptr, policy: int): void = "mac#talweg_c_gc_set_policy"

assume gc_engine_vtype = ptr

implement gc_engine_create (total, ppb, policy) = talweg_c_gc_create (total, ppb, policy)
implement gc_engine_destroy (gc) = talweg_c_gc_destroy (gc)
implement gc_engine_invalidate_page (gc, blk) = talweg_c_gc_invalidate_page (gc, blk)
implement gc_engine_validate_page (gc, blk) = talweg_c_gc_validate_page (gc, blk)
implement gc_engine_get_invalid_count (gc, blk) = talweg_c_gc_get_invalid_count (gc, blk)
implement gc_engine_get_valid_count (gc, blk) = talweg_c_gc_get_valid_count (gc, blk)
implement gc_engine_select_victim (gc) = talweg_c_gc_select_victim (gc)
implement gc_engine_reset_block (gc, blk) = talweg_c_gc_reset_block (gc, blk)
implement gc_engine_set_policy (gc, policy) = talweg_c_gc_set_policy (gc, policy)

%{
void *talweg_c_gc_create(uint32_t total, uint32_t ppb, int policy) {
    if (total == 0 || ppb == 0) return NULL;
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)calloc(1, sizeof(talweg_ats_gc_t));
    if (!gc) return NULL;

    gc->total_blocks = total;
    gc->pages_per_block = ppb;
    gc->policy = policy;
    gc->valid_counts = (uint32_t *)calloc(total, sizeof(uint32_t));
    gc->invalid_counts = (uint32_t *)calloc(total, sizeof(uint32_t));
    gc->last_mod_time = (uint64_t *)calloc(total, sizeof(uint64_t));

    if (!gc->valid_counts || !gc->invalid_counts || !gc->last_mod_time) {
        if (gc->valid_counts) free(gc->valid_counts);
        if (gc->invalid_counts) free(gc->invalid_counts);
        if (gc->last_mod_time) free(gc->last_mod_time);
        free(gc);
        return NULL;
    }
    gc->logical_clock = 1;
    return (void *)gc;
}

void talweg_c_gc_destroy(void *p) {
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)p;
    if (gc) {
        if (gc->valid_counts) free(gc->valid_counts);
        if (gc->invalid_counts) free(gc->invalid_counts);
        if (gc->last_mod_time) free(gc->last_mod_time);
        free(gc);
    }
}

void talweg_c_gc_invalidate_page(void *p, uint32_t blk) {
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)p;
    if (!gc || blk >= gc->total_blocks) return;
    if (gc->valid_counts[blk] > 0) {
        gc->valid_counts[blk]--;
    }
    gc->invalid_counts[blk]++;
    gc->last_mod_time[blk] = ++gc->logical_clock;
}

void talweg_c_gc_validate_page(void *p, uint32_t blk) {
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)p;
    if (!gc || blk >= gc->total_blocks) return;
    gc->valid_counts[blk]++;
    gc->last_mod_time[blk] = ++gc->logical_clock;
}

uint32_t talweg_c_gc_get_invalid_count(void *p, uint32_t blk) {
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)p;
    if (!gc || blk >= gc->total_blocks) return 0;
    return gc->invalid_counts[blk];
}

uint32_t talweg_c_gc_get_valid_count(void *p, uint32_t blk) {
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)p;
    if (!gc || blk >= gc->total_blocks) return 0;
    return gc->valid_counts[blk];
}

uint32_t talweg_c_gc_select_victim(void *p) {
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)p;
    if (!gc) return TALWEG_INVALID_BLOCK;

    uint32_t best_blk = TALWEG_INVALID_BLOCK;

    if (gc->policy == TALWEG_GC_GREEDY) {
        uint32_t max_invalid = 0;
        for (uint32_t i = 0; i < gc->total_blocks; i++) {
            if (gc->invalid_counts[i] > max_invalid) {
                max_invalid = gc->invalid_counts[i];
                best_blk = i;
            }
        }
    } else {
        /* Cost-Benefit policy: (1 - u) * age / (1 + u) */
        double best_cost_benefit = -1.0;
        for (uint32_t i = 0; i < gc->total_blocks; i++) {
            if (gc->invalid_counts[i] == 0) continue;
            double u = (double)gc->valid_counts[i] / (double)gc->pages_per_block;
            if (u >= 1.0) continue;
            double age = (double)(gc->logical_clock - gc->last_mod_time[i] + 1);
            double cb = ((1.0 - u) * age) / (1.0 + u);
            if (cb > best_cost_benefit) {
                best_cost_benefit = cb;
                best_blk = i;
            }
        }
    }

    /* Fallback if no invalid pages found */
    if (best_blk == TALWEG_INVALID_BLOCK) {
        uint32_t max_inv = 0;
        for (uint32_t i = 0; i < gc->total_blocks; i++) {
            if (gc->invalid_counts[i] >= max_inv) {
                max_inv = gc->invalid_counts[i];
                best_blk = i;
            }
        }
    }

    return best_blk;
}

void talweg_c_gc_reset_block(void *p, uint32_t blk) {
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)p;
    if (!gc || blk >= gc->total_blocks) return;
    gc->valid_counts[blk] = 0;
    gc->invalid_counts[blk] = 0;
    gc->last_mod_time[blk] = 0;
}

void talweg_c_gc_set_policy(void *p, int policy) {
    talweg_ats_gc_t *gc = (talweg_ats_gc_t *)p;
    if (gc) gc->policy = policy;
}
%}
