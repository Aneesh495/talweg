#ifndef TALWEG_JOURNAL_H
#define TALWEG_JOURNAL_H

#include "talweg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    TALWEG_WAL_REC_NOP = 0,
    TALWEG_WAL_REC_WRITE = 1,
    TALWEG_WAL_REC_TRIM = 2,
    TALWEG_WAL_REC_ERASE = 3,
    TALWEG_WAL_REC_CHECKPOINT = 4
} talweg_wal_type_t;

typedef struct {
    uint64_t seq;
    uint32_t type;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    uint32_t checksum;
} talweg_wal_record_t;

typedef struct talweg_wal_log talweg_wal_log_t;

typedef void (*talweg_wal_replay_cb)(const talweg_wal_record_t *rec, void *user_data);

int talweg_wal_open(talweg_wal_log_t **out_wal, const char *path);
void talweg_wal_close(talweg_wal_log_t *wal);

int talweg_wal_append(talweg_wal_log_t *wal, uint32_t type, uint32_t a1, uint32_t a2, uint32_t a3, uint64_t *out_seq);
int talweg_wal_sync(talweg_wal_log_t *wal);
int talweg_wal_replay(const char *path, talweg_wal_replay_cb cb, void *user_data, uint64_t *out_replayed);

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_JOURNAL_H */
