(*
** Talweg: High-Assurance Flash Translation Layer
** Logical-to-Physical (L2P) Mapping Table Implementation
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"
staload "./../SATS/talweg_lba_map.sats"

%{^
#include "talweg_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t capacity;
    uint32_t mapped_count;
    uint32_t *entries;
} talweg_ats_lba_tbl_t;
%}

extern fun talweg_c_lba_map_create (cap: uint): ptr = "mac#talweg_c_lba_map_create"
extern fun talweg_c_lba_map_destroy (p: ptr): void = "mac#talweg_c_lba_map_destroy"
extern fun talweg_c_lba_map_lookup (p: ptr, lba: uint): uint = "mac#talweg_c_lba_map_lookup"
extern fun talweg_c_lba_map_is_mapped (p: ptr, lba: uint): bool = "mac#talweg_c_lba_map_is_mapped"
extern fun talweg_c_lba_map_count_mapped (p: ptr): uint = "mac#talweg_c_lba_map_count_mapped"
extern fun talweg_c_lba_map_get_capacity (p: ptr): uint = "mac#talweg_c_lba_map_get_capacity"
extern fun talweg_c_lba_map_update (p: ptr, lba: uint, new_ppn: uint): uint = "mac#talweg_c_lba_map_update"
extern fun talweg_c_lba_map_unmap (p: ptr, lba: uint): uint = "mac#talweg_c_lba_map_unmap"

assume lba_map_table_vtype = ptr

implement lba_map_create (cap) = talweg_c_lba_map_create (cap)
implement lba_map_destroy (tbl) = talweg_c_lba_map_destroy (tbl)
implement lba_map_lookup (tbl, lba) = talweg_c_lba_map_lookup (tbl, lba)
implement lba_map_is_mapped (tbl, lba) = talweg_c_lba_map_is_mapped (tbl, lba)
implement lba_map_count_mapped (tbl) = talweg_c_lba_map_count_mapped (tbl)
implement lba_map_get_capacity (tbl) = talweg_c_lba_map_get_capacity (tbl)
implement lba_map_update (tbl, lba, new_ppn) = talweg_c_lba_map_update (tbl, lba, new_ppn)
implement lba_map_unmap (tbl, lba) = talweg_c_lba_map_unmap (tbl, lba)

%{
void *talweg_c_lba_map_create(uint32_t capacity) {
    talweg_ats_lba_tbl_t *tbl = (talweg_ats_lba_tbl_t *)malloc(sizeof(talweg_ats_lba_tbl_t));
    if (!tbl) return NULL;
    tbl->capacity = capacity;
    tbl->mapped_count = 0;
    tbl->entries = (uint32_t *)malloc(capacity * sizeof(uint32_t));
    if (!tbl->entries) {
        free(tbl);
        return NULL;
    }
    for (uint32_t i = 0; i < capacity; i++) {
        tbl->entries[i] = TALWEG_UNMAPPED_PPN;
    }
    return (void *)tbl;
}

void talweg_c_lba_map_destroy(void *p) {
    talweg_ats_lba_tbl_t *tbl = (talweg_ats_lba_tbl_t *)p;
    if (tbl) {
        if (tbl->entries) free(tbl->entries);
        free(tbl);
    }
}

uint32_t talweg_c_lba_map_lookup(void *p, uint32_t lba) {
    talweg_ats_lba_tbl_t *tbl = (talweg_ats_lba_tbl_t *)p;
    if (!tbl || lba >= tbl->capacity) return TALWEG_UNMAPPED_PPN;
    return tbl->entries[lba];
}

bool talweg_c_lba_map_is_mapped(void *p, uint32_t lba) {
    talweg_ats_lba_tbl_t *tbl = (talweg_ats_lba_tbl_t *)p;
    if (!tbl || lba >= tbl->capacity) return false;
    return tbl->entries[lba] != TALWEG_UNMAPPED_PPN;
}

uint32_t talweg_c_lba_map_count_mapped(void *p) {
    talweg_ats_lba_tbl_t *tbl = (talweg_ats_lba_tbl_t *)p;
    return tbl ? tbl->mapped_count : 0;
}

uint32_t talweg_c_lba_map_get_capacity(void *p) {
    talweg_ats_lba_tbl_t *tbl = (talweg_ats_lba_tbl_t *)p;
    return tbl ? tbl->capacity : 0;
}

uint32_t talweg_c_lba_map_update(void *p, uint32_t lba, uint32_t new_ppn) {
    talweg_ats_lba_tbl_t *tbl = (talweg_ats_lba_tbl_t *)p;
    if (!tbl || lba >= tbl->capacity) return TALWEG_INVALID_PPN;
    uint32_t old_ppn = tbl->entries[lba];
    tbl->entries[lba] = new_ppn;
    if (old_ppn == TALWEG_UNMAPPED_PPN && new_ppn != TALWEG_UNMAPPED_PPN) {
        tbl->mapped_count++;
    } else if (old_ppn != TALWEG_UNMAPPED_PPN && new_ppn == TALWEG_UNMAPPED_PPN) {
        if (tbl->mapped_count > 0) tbl->mapped_count--;
    }
    return old_ppn;
}

uint32_t talweg_c_lba_map_unmap(void *p, uint32_t lba) {
    return talweg_c_lba_map_update(p, lba, TALWEG_UNMAPPED_PPN);
}
%}
