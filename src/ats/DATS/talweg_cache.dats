(*
** Talweg: High-Assurance Flash Translation Layer
** Linear-Pinned Page Cache Implementation
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"
staload "./../SATS/talweg_cache.sats"

%{^
#include "talweg_types.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    uint32_t lba;
    uint8_t *buffer;
    uint32_t pin_count;
    bool is_dirty;
    bool is_valid;
    uint64_t last_access;
} talweg_cache_frame_t;

typedef struct {
    uint32_t capacity;
    uint32_t page_size;
    uint32_t dirty_count;
    uint64_t hits;
    uint64_t misses;
    uint64_t access_clock;
    talweg_cache_frame_t *frames;
} talweg_ats_cache_t;
%}

extern fun talweg_c_cache_create (cap: uint, ps: uint): ptr = "mac#talweg_c_cache_create"
extern fun talweg_c_cache_destroy (p: ptr): void = "mac#talweg_c_cache_destroy"
extern fun talweg_c_cache_lookup (p: ptr, lba: uint): int = "mac#talweg_c_cache_lookup"
extern fun talweg_c_cache_pin (p: ptr, lba: uint, out_buf: &ptr? >> ptr): int = "mac#talweg_c_cache_pin"
extern fun talweg_c_cache_unpin (p: ptr, lba: uint, is_dirty: bool): int = "mac#talweg_c_cache_unpin"
extern fun talweg_c_cache_get_dirty_count (p: ptr): uint = "mac#talweg_c_cache_get_dirty_count"
extern fun talweg_c_cache_get_hit_count (p: ptr): ullint = "mac#talweg_c_cache_get_hit_count"
extern fun talweg_c_cache_get_miss_count (p: ptr): ullint = "mac#talweg_c_cache_get_miss_count"
extern fun talweg_c_cache_invalidate (p: ptr, lba: uint): void = "mac#talweg_c_cache_invalidate"

assume cache_pool_vtype = ptr

implement cache_pool_create (cap, ps) = talweg_c_cache_create (cap, ps)
implement cache_pool_destroy (c) = talweg_c_cache_destroy (c)
implement cache_pool_lookup (c, lba) = talweg_c_cache_lookup (c, lba)
implement cache_pool_pin (c, lba, out_buf) = talweg_c_cache_pin (c, lba, out_buf)
implement cache_pool_unpin (c, lba, is_dirty) = talweg_c_cache_unpin (c, lba, is_dirty)
implement cache_pool_get_dirty_count (c) = talweg_c_cache_get_dirty_count (c)
implement cache_pool_get_hit_count (c) = talweg_c_cache_get_hit_count (c)
implement cache_pool_get_miss_count (c) = talweg_c_cache_get_miss_count (c)
implement cache_pool_invalidate (c, lba) = talweg_c_cache_invalidate (c, lba)

%{
void *talweg_c_cache_create(uint32_t cap, uint32_t ps) {
    if (cap == 0) cap = 64;
    if (ps == 0) ps = 4096;
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)calloc(1, sizeof(talweg_ats_cache_t));
    if (!c) return NULL;

    c->capacity = cap;
    c->page_size = ps;
    c->frames = (talweg_cache_frame_t *)calloc(cap, sizeof(talweg_cache_frame_t));
    if (!c->frames) {
        free(c);
        return NULL;
    }

    for (uint32_t i = 0; i < cap; i++) {
        c->frames[i].buffer = (uint8_t *)malloc(ps);
        if (!c->frames[i].buffer) {
            for (uint32_t j = 0; j < i; j++) free(c->frames[j].buffer);
            free(c->frames);
            free(c);
            return NULL;
        }
        c->frames[i].lba = 0xFFFFFFFFU;
        c->frames[i].is_valid = false;
        c->frames[i].is_dirty = false;
        c->frames[i].pin_count = 0;
        c->frames[i].last_access = 0;
    }
    return (void *)c;
}

void talweg_c_cache_destroy(void *p) {
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)p;
    if (c) {
        if (c->frames) {
            for (uint32_t i = 0; i < c->capacity; i++) {
                if (c->frames[i].buffer) free(c->frames[i].buffer);
            }
            free(c->frames);
        }
        free(c);
    }
}

int talweg_c_cache_lookup(void *p, uint32_t lba) {
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)p;
    if (!c) return -1;
    for (uint32_t i = 0; i < c->capacity; i++) {
        if (c->frames[i].is_valid && c->frames[i].lba == lba) {
            c->hits++;
            c->frames[i].last_access = ++c->access_clock;
            return (int)i;
        }
    }
    c->misses++;
    return -1;
}

int talweg_c_cache_pin(void *p, uint32_t lba, void **out_buf) {
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)p;
    if (!c || !out_buf) return TALWEG_ERR_INVAL;

    /* Check if already present */
    for (uint32_t i = 0; i < c->capacity; i++) {
        if (c->frames[i].is_valid && c->frames[i].lba == lba) {
            c->hits++;
            c->frames[i].pin_count++;
            c->frames[i].last_access = ++c->access_clock;
            *out_buf = c->frames[i].buffer;
            return TALWEG_OK;
        }
    }

    /* Not present: find an unpinned frame to evict */
    c->misses++;
    int victim = -1;
    uint64_t oldest = UINT64_MAX;

    for (uint32_t i = 0; i < c->capacity; i++) {
        if (!c->frames[i].is_valid) {
            victim = (int)i;
            break;
        }
        if (c->frames[i].pin_count == 0 && c->frames[i].last_access < oldest) {
            oldest = c->frames[i].last_access;
            victim = (int)i;
        }
    }

    if (victim < 0) {
        *out_buf = NULL;
        return TALWEG_ERR_NOMEM; /* All frames pinned */
    }

    talweg_cache_frame_t *f = &c->frames[victim];
    if (f->is_valid && f->is_dirty) {
        if (c->dirty_count > 0) c->dirty_count--;
    }

    f->lba = lba;
    f->is_valid = true;
    f->is_dirty = false;
    f->pin_count = 1;
    f->last_access = ++c->access_clock;
    *out_buf = f->buffer;

    return TALWEG_OK;
}

int talweg_c_cache_unpin(void *p, uint32_t lba, bool is_dirty) {
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)p;
    if (!c) return TALWEG_ERR_INVAL;

    for (uint32_t i = 0; i < c->capacity; i++) {
        if (c->frames[i].is_valid && c->frames[i].lba == lba) {
            if (c->frames[i].pin_count > 0) {
                c->frames[i].pin_count--;
            }
            if (is_dirty && !c->frames[i].is_dirty) {
                c->frames[i].is_dirty = true;
                c->dirty_count++;
            }
            return TALWEG_OK;
        }
    }
    return TALWEG_ERR_NOT_FOUND;
}

uint32_t talweg_c_cache_get_dirty_count(void *p) {
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)p;
    return c ? c->dirty_count : 0;
}

unsigned long long talweg_c_cache_get_hit_count(void *p) {
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)p;
    return c ? c->hits : 0;
}

unsigned long long talweg_c_cache_get_miss_count(void *p) {
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)p;
    return c ? c->misses : 0;
}

void talweg_c_cache_invalidate(void *p, uint32_t lba) {
    talweg_ats_cache_t *c = (talweg_ats_cache_t *)p;
    if (!c) return;
    for (uint32_t i = 0; i < c->capacity; i++) {
        if (c->frames[i].is_valid && c->frames[i].lba == lba) {
            if (c->frames[i].is_dirty && c->dirty_count > 0) {
                c->dirty_count--;
            }
            c->frames[i].is_valid = false;
            c->frames[i].is_dirty = false;
            c->frames[i].pin_count = 0;
            break;
        }
    }
}
%}
