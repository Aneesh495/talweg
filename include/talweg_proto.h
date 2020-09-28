#ifndef TALWEG_PROTO_H
#define TALWEG_PROTO_H

#include "talweg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TALWEG_PROTO_MAGIC 0x54574750U /* "TWGP" */

typedef enum {
    TALWEG_CMD_PING = 1,
    TALWEG_CMD_READ = 2,
    TALWEG_CMD_WRITE = 3,
    TALWEG_CMD_TRIM = 4,
    TALWEG_CMD_FLUSH = 5,
    TALWEG_CMD_STATS = 6,
    TALWEG_CMD_GC = 7,
    TALWEG_RESP_OK = 100,
    TALWEG_RESP_ERR = 101
} talweg_cmd_type_t;

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint16_t cmd;
    uint16_t flags;
    uint64_t tag;
    uint32_t lba;
    uint32_t count;
    int32_t  status;
    uint32_t payload_len;
    uint32_t crc32;
} talweg_proto_hdr_t;

typedef struct {
    uint64_t user_writes;
    uint64_t flash_writes;
    uint64_t gc_cycles;
    uint32_t waf_mille;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint32_t free_blocks;
    uint32_t bad_blocks;
    uint32_t min_erase;
    uint32_t max_erase;
    uint32_t wear_delta;
    uint32_t mapped_lba_count;
} talweg_proto_stats_payload_t;

#pragma pack(pop)

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_PROTO_H */
