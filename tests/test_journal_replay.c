#include "talweg_journal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>

typedef struct {
    uint32_t writes;
    uint32_t trims;
    uint32_t erases;
    uint64_t last_seq;
} test_replay_state_t;

static void test_cb(const talweg_wal_record_t *rec, void *user_data) {
    test_replay_state_t *st = (test_replay_state_t *)user_data;
    assert(rec->seq > st->last_seq);
    st->last_seq = rec->seq;

    if (rec->type == TALWEG_WAL_REC_WRITE) st->writes++;
    else if (rec->type == TALWEG_WAL_REC_TRIM) st->trims++;
    else if (rec->type == TALWEG_WAL_REC_ERASE) st->erases++;
}

int main(void) {
    printf("=== Test: Write-Ahead Journal Serialization & Replay ===\n");

    const char *jpath = "test_run.journal";
    unlink(jpath);

    /* 1. Create and append records */
    talweg_wal_log_t *wal = NULL;
    assert(talweg_wal_open(&wal, jpath) == TALWEG_OK);

    for (uint32_t i = 1; i <= 50; i++) {
        uint64_t seq = 0;
        assert(talweg_wal_append(wal, TALWEG_WAL_REC_WRITE, i, i * 10, 0, &seq) == TALWEG_OK);
        assert(seq == i);
    }
    for (uint32_t i = 51; i <= 70; i++) {
        uint64_t seq = 0;
        assert(talweg_wal_append(wal, TALWEG_WAL_REC_TRIM, i, 0, 0, &seq) == TALWEG_OK);
        assert(seq == i);
    }
    for (uint32_t i = 71; i <= 80; i++) {
        uint64_t seq = 0;
        assert(talweg_wal_append(wal, TALWEG_WAL_REC_ERASE, i % 10, i, 0, &seq) == TALWEG_OK);
        assert(seq == i);
    }
    talweg_wal_close(wal);

    /* 2. Replay all 80 records */
    test_replay_state_t st;
    st.writes = 0;
    st.trims = 0;
    st.erases = 0;
    st.last_seq = 0;

    uint64_t replayed = 0;
    assert(talweg_wal_replay(jpath, test_cb, &st, &replayed) == TALWEG_OK);
    assert(replayed == 80);
    assert(st.writes == 50);
    assert(st.trims == 20);
    assert(st.erases == 10);

    /* 3. Corrupt tail to simulate torn write */
    int fd = open(jpath, O_RDWR);
    assert(fd >= 0);
    off_t sz = lseek(fd, -10, SEEK_END);
    assert(sz > 0);
    char garbage[10] = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    assert(write(fd, garbage, 10) == 10);
    close(fd);

    /* 4. Replay stops at valid boundary before corruption */
    st.writes = 0;
    st.trims = 0;
    st.erases = 0;
    st.last_seq = 0;
    replayed = 0;
    assert(talweg_wal_replay(jpath, test_cb, &st, &replayed) == TALWEG_OK);
    assert(replayed == 79); /* Exact prefix recovered */

    unlink(jpath);
    printf("PASS: All journal serialization and recovery tests passed.\n");
    return 0;
}
