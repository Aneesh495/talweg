(*
** Talweg: High-Assurance Flash Translation Layer
** Write-Ahead Journal Implementation and Crash Replay
*)

#define ATS_DYNLOADFLAG 0

#include "share/atspre_staload.hats"
staload "./../SATS/talweg_geom.sats"
staload "./../SATS/talweg_journal.sats"

%{^
#include "talweg_types.h"
#include "talweg_crc32.h"
#include <stdlib.h>
#include <string.h>

typedef enum {
    TALWEG_JRNL_NOP = 0,
    TALWEG_JRNL_WRITE = 1,
    TALWEG_JRNL_TRIM = 2,
    TALWEG_JRNL_ERASE = 3
} talweg_journal_type_t;

typedef struct {
    uint64_t seq;
    uint32_t type;
    uint32_t arg1; /* lba or blk */
    uint32_t arg2; /* new_ppn or erase_count */
    uint32_t arg3; /* old_ppn */
    uint32_t checksum;
} talweg_journal_record_t;

typedef struct {
    uint32_t capacity;
    uint32_t count;
    uint32_t head;
    uint32_t tail;
    uint64_t current_seq;
    talweg_journal_record_t *records;
} talweg_ats_journal_t;
%}

extern fun talweg_c_journal_create (cap: uint): ptr = "mac#talweg_c_journal_create"
extern fun talweg_c_journal_destroy (p: ptr): void = "mac#talweg_c_journal_destroy"
extern fun talweg_c_journal_append_write (p: ptr, lba: uint, new_ppn: uint, old_ppn: uint): ullint = "mac#talweg_c_journal_append_write"
extern fun talweg_c_journal_append_trim (p: ptr, lba: uint, old_ppn: uint): ullint = "mac#talweg_c_journal_append_trim"
extern fun talweg_c_journal_append_erase (p: ptr, blk: uint, count: uint): ullint = "mac#talweg_c_journal_append_erase"
extern fun talweg_c_journal_get_sequence (p: ptr): ullint = "mac#talweg_c_journal_get_sequence"
extern fun talweg_c_journal_get_count (p: ptr): uint = "mac#talweg_c_journal_get_count"
extern fun talweg_c_journal_clear (p: ptr): void = "mac#talweg_c_journal_clear"

assume journal_engine_vtype = ptr

implement journal_engine_create (cap) = talweg_c_journal_create (cap)
implement journal_engine_destroy (j) = talweg_c_journal_destroy (j)
implement journal_append_write (j, lba, new_ppn, old_ppn) = talweg_c_journal_append_write (j, lba, new_ppn, old_ppn)
implement journal_append_trim (j, lba, old_ppn) = talweg_c_journal_append_trim (j, lba, old_ppn)
implement journal_append_erase (j, blk, count) = talweg_c_journal_append_erase (j, blk, count)
implement journal_get_sequence (j) = talweg_c_journal_get_sequence (j)
implement journal_get_count (j) = talweg_c_journal_get_count (j)
implement journal_clear (j) = talweg_c_journal_clear (j)

%{
static uint32_t talweg_calc_record_crc(const talweg_journal_record_t *rec) {
    /* Compute CRC over first 5 fields (excluding checksum field itself) */
    return talweg_crc32c(0xFFFFFFFFU, rec, sizeof(uint64_t) + 4 * sizeof(uint32_t)) ^ 0xFFFFFFFFU;
}

void *talweg_c_journal_create(uint32_t cap) {
    if (cap == 0) cap = 1024;
    talweg_ats_journal_t *j = (talweg_ats_journal_t *)calloc(1, sizeof(talweg_ats_journal_t));
    if (!j) return NULL;
    j->capacity = cap;
    j->records = (talweg_journal_record_t *)calloc(cap, sizeof(talweg_journal_record_t));
    if (!j->records) {
        free(j);
        return NULL;
    }
    j->count = 0;
    j->head = 0;
    j->tail = 0;
    j->current_seq = 0;
    return (void *)j;
}

void talweg_c_journal_destroy(void *p) {
    talweg_ats_journal_t *j = (talweg_ats_journal_t *)p;
    if (j) {
        if (j->records) free(j->records);
        free(j);
    }
}

unsigned long long talweg_c_journal_append_write(void *p, uint32_t lba, uint32_t new_ppn, uint32_t old_ppn) {
    talweg_ats_journal_t *j = (talweg_ats_journal_t *)p;
    if (!j) return 0;

    j->current_seq++;
    talweg_journal_record_t *rec = &j->records[j->tail];
    rec->seq = j->current_seq;
    rec->type = TALWEG_JRNL_WRITE;
    rec->arg1 = lba;
    rec->arg2 = new_ppn;
    rec->arg3 = old_ppn;
    rec->checksum = talweg_calc_record_crc(rec);

    j->tail = (j->tail + 1) % j->capacity;
    if (j->count < j->capacity) {
        j->count++;
    } else {
        j->head = (j->head + 1) % j->capacity;
    }
    return j->current_seq;
}

unsigned long long talweg_c_journal_append_trim(void *p, uint32_t lba, uint32_t old_ppn) {
    talweg_ats_journal_t *j = (talweg_ats_journal_t *)p;
    if (!j) return 0;

    j->current_seq++;
    talweg_journal_record_t *rec = &j->records[j->tail];
    rec->seq = j->current_seq;
    rec->type = TALWEG_JRNL_TRIM;
    rec->arg1 = lba;
    rec->arg2 = TALWEG_UNMAPPED_PPN;
    rec->arg3 = old_ppn;
    rec->checksum = talweg_calc_record_crc(rec);

    j->tail = (j->tail + 1) % j->capacity;
    if (j->count < j->capacity) {
        j->count++;
    } else {
        j->head = (j->head + 1) % j->capacity;
    }
    return j->current_seq;
}

unsigned long long talweg_c_journal_append_erase(void *p, uint32_t blk, uint32_t count) {
    talweg_ats_journal_t *j = (talweg_ats_journal_t *)p;
    if (!j) return 0;

    j->current_seq++;
    talweg_journal_record_t *rec = &j->records[j->tail];
    rec->seq = j->current_seq;
    rec->type = TALWEG_JRNL_ERASE;
    rec->arg1 = blk;
    rec->arg2 = count;
    rec->arg3 = 0;
    rec->checksum = talweg_calc_record_crc(rec);

    j->tail = (j->tail + 1) % j->capacity;
    if (j->count < j->capacity) {
        j->count++;
    } else {
        j->head = (j->head + 1) % j->capacity;
    }
    return j->current_seq;
}

unsigned long long talweg_c_journal_get_sequence(void *p) {
    talweg_ats_journal_t *j = (talweg_ats_journal_t *)p;
    return j ? j->current_seq : 0;
}

uint32_t talweg_c_journal_get_count(void *p) {
    talweg_ats_journal_t *j = (talweg_ats_journal_t *)p;
    return j ? j->count : 0;
}

void talweg_c_journal_clear(void *p) {
    talweg_ats_journal_t *j = (talweg_ats_journal_t *)p;
    if (j) {
        j->count = 0;
        j->head = 0;
        j->tail = 0;
    }
}
%}
