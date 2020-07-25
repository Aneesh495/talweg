(*
** Talweg: High-Assurance Flash Translation Layer
** Wear-Leveling Dual-Pool Engine Implementation
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"
staload "./../SATS/talweg_wear.sats"

%{^
#include "talweg_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t total_blocks;
    uint32_t delta_threshold;
    uint32_t *erase_counts;
    uint32_t min_erase;
    uint32_t max_erase;
    uint64_t total_erases;
} talweg_ats_wear_tracker_t;
%}

extern fun talweg_c_wear_create (total: uint, thresh: uint): ptr = "mac#talweg_c_wear_create"
extern fun talweg_c_wear_destroy (p: ptr): void = "mac#talweg_c_wear_destroy"
extern fun talweg_c_wear_record_erase (p: ptr, blk: uint): uint = "mac#talweg_c_wear_record_erase"
extern fun talweg_c_wear_get_erase_count (p: ptr, blk: uint): uint = "mac#talweg_c_wear_get_erase_count"
extern fun talweg_c_wear_get_min_erase (p: ptr): uint = "mac#talweg_c_wear_get_min_erase"
extern fun talweg_c_wear_get_max_erase (p: ptr): uint = "mac#talweg_c_wear_get_max_erase"
extern fun talweg_c_wear_get_wear_delta (p: ptr): uint = "mac#talweg_c_wear_get_wear_delta"
extern fun talweg_c_wear_needs_static_rebalance (p: ptr): bool = "mac#talweg_c_wear_needs_static_rebalance"
extern fun talweg_c_wear_find_cold_candidate (p: ptr): uint = "mac#talweg_c_wear_find_cold_candidate"

assume wear_tracker_vtype = ptr

implement wear_tracker_create (total, thresh) = talweg_c_wear_create (total, thresh)
implement wear_tracker_destroy (wt) = talweg_c_wear_destroy (wt)
implement wear_tracker_record_erase (wt, blk) = talweg_c_wear_record_erase (wt, blk)
implement wear_tracker_get_erase_count (wt, blk) = talweg_c_wear_get_erase_count (wt, blk)
implement wear_tracker_get_min_erase (wt) = talweg_c_wear_get_min_erase (wt)
implement wear_tracker_get_max_erase (wt) = talweg_c_wear_get_max_erase (wt)
implement wear_tracker_get_wear_delta (wt) = talweg_c_wear_get_wear_delta (wt)
implement wear_tracker_needs_static_rebalance (wt) = talweg_c_wear_needs_static_rebalance (wt)
implement wear_tracker_find_cold_candidate (wt) = talweg_c_wear_find_cold_candidate (wt)

%{
void *talweg_c_wear_create(uint32_t total, uint32_t thresh) {
    if (total == 0) return NULL;
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)calloc(1, sizeof(talweg_ats_wear_tracker_t));
    if (!wt) return NULL;
    wt->total_blocks = total;
    wt->delta_threshold = (thresh > 0) ? thresh : 32;
    wt->erase_counts = (uint32_t *)calloc(total, sizeof(uint32_t));
    if (!wt->erase_counts) {
        free(wt);
        return NULL;
    }
    wt->min_erase = 0;
    wt->max_erase = 0;
    wt->total_erases = 0;
    return (void *)wt;
}

void talweg_c_wear_destroy(void *p) {
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)p;
    if (wt) {
        if (wt->erase_counts) free(wt->erase_counts);
        free(wt);
    }
}

uint32_t talweg_c_wear_record_erase(void *p, uint32_t blk) {
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)p;
    if (!wt || blk >= wt->total_blocks) return 0;

    wt->erase_counts[blk]++;
    wt->total_erases++;
    uint32_t count = wt->erase_counts[blk];

    if (count > wt->max_erase) {
        wt->max_erase = count;
    }

    /* Recalculate min if needed */
    if (count - 1 == wt->min_erase) {
        uint32_t new_min = UINT32_MAX;
        for (uint32_t i = 0; i < wt->total_blocks; i++) {
            if (wt->erase_counts[i] < new_min) {
                new_min = wt->erase_counts[i];
            }
        }
        wt->min_erase = new_min;
    }

    return count;
}

uint32_t talweg_c_wear_get_erase_count(void *p, uint32_t blk) {
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)p;
    if (!wt || blk >= wt->total_blocks) return 0;
    return wt->erase_counts[blk];
}

uint32_t talweg_c_wear_get_min_erase(void *p) {
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)p;
    return wt ? wt->min_erase : 0;
}

uint32_t talweg_c_wear_get_max_erase(void *p) {
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)p;
    return wt ? wt->max_erase : 0;
}

uint32_t talweg_c_wear_get_wear_delta(void *p) {
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)p;
    if (!wt) return 0;
    return (wt->max_erase >= wt->min_erase) ? (wt->max_erase - wt->min_erase) : 0;
}

bool talweg_c_wear_needs_static_rebalance(void *p) {
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)p;
    if (!wt) return false;
    return (wt->max_erase - wt->min_erase) >= wt->delta_threshold;
}

uint32_t talweg_c_wear_find_cold_candidate(void *p) {
    talweg_ats_wear_tracker_t *wt = (talweg_ats_wear_tracker_t *)p;
    if (!wt) return TALWEG_INVALID_BLOCK;
    for (uint32_t i = 0; i < wt->total_blocks; i++) {
        if (wt->erase_counts[i] == wt->min_erase) {
            return i;
        }
    }
    return 0;
}
%}
