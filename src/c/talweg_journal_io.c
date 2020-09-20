#include "talweg_journal.h"
#include "talweg_crc32.h"
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define WAL_MAGIC 0x5457414CU /* "TWAL" */

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t last_seq;
} talweg_wal_header_t;

struct talweg_wal_log {
    int fd;
    uint64_t seq;
};

int talweg_wal_open(talweg_wal_log_t **out_wal, const char *path) {
    if (!out_wal || !path) return TALWEG_ERR_INVAL;

    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) return TALWEG_ERR_MEDIA_IO;

    talweg_wal_header_t hdr;
    ssize_t rd = read(fd, &hdr, sizeof(hdr));
    uint64_t initial_seq = 0;

    if (rd == 0) {
        /* New file */
        hdr.magic = WAL_MAGIC;
        hdr.version = 1;
        hdr.last_seq = 0;
        if (write(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
            close(fd);
            return TALWEG_ERR_MEDIA_IO;
        }
    } else if (rd == (ssize_t)sizeof(hdr)) {
        if (hdr.magic != WAL_MAGIC) {
            close(fd);
            return TALWEG_ERR_CORRUPT_METADATA;
        }
        initial_seq = hdr.last_seq;
        lseek(fd, 0, SEEK_END);
    } else {
        close(fd);
        return TALWEG_ERR_CORRUPT_METADATA;
    }

    talweg_wal_log_t *w = (talweg_wal_log_t *)calloc(1, sizeof(talweg_wal_log_t));
    if (!w) {
        close(fd);
        return TALWEG_ERR_NOMEM;
    }
    w->fd = fd;
    w->seq = initial_seq;

    *out_wal = w;
    return TALWEG_OK;
}

void talweg_wal_close(talweg_wal_log_t *wal) {
    if (wal) {
        if (wal->fd >= 0) {
            fsync(wal->fd);
            close(wal->fd);
        }
        free(wal);
    }
}

int talweg_wal_append(talweg_wal_log_t *wal, uint32_t type, uint32_t a1, uint32_t a2, uint32_t a3, uint64_t *out_seq) {
    if (!wal || wal->fd < 0) return TALWEG_ERR_INVAL;

    wal->seq++;
    talweg_wal_record_t rec;
    rec.seq = wal->seq;
    rec.type = type;
    rec.arg1 = a1;
    rec.arg2 = a2;
    rec.arg3 = a3;
    rec.checksum = talweg_crc32c(0xFFFFFFFFU, &rec, sizeof(uint64_t) + 4 * sizeof(uint32_t)) ^ 0xFFFFFFFFU;

    if (write(wal->fd, &rec, sizeof(rec)) != (ssize_t)sizeof(rec)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    if (out_seq) *out_seq = wal->seq;
    return TALWEG_OK;
}

int talweg_wal_sync(talweg_wal_log_t *wal) {
    if (!wal || wal->fd < 0) return TALWEG_ERR_INVAL;
    return fsync(wal->fd) == 0 ? TALWEG_OK : TALWEG_ERR_MEDIA_IO;
}

int talweg_wal_replay(const char *path, talweg_wal_replay_cb cb, void *user_data, uint64_t *out_replayed) {
    if (!path || !cb) return TALWEG_ERR_INVAL;

    int fd = open(path, O_RDONLY);
    if (fd < 0) return TALWEG_ERR_MEDIA_IO;

    talweg_wal_header_t hdr;
    if (read(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr) || hdr.magic != WAL_MAGIC) {
        close(fd);
        return TALWEG_ERR_CORRUPT_METADATA;
    }

    uint64_t count = 0;
    talweg_wal_record_t rec;
    while (read(fd, &rec, sizeof(rec)) == (ssize_t)sizeof(rec)) {
        uint32_t expected_crc = talweg_crc32c(0xFFFFFFFFU, &rec, sizeof(uint64_t) + 4 * sizeof(uint32_t)) ^ 0xFFFFFFFFU;
        if (rec.checksum != expected_crc) {
            /* Corrupted or truncated tail record from simulated power cut */
            break;
        }
        cb(&rec, user_data);
        count++;
    }

    close(fd);
    if (out_replayed) *out_replayed = count;
    return TALWEG_OK;
}
