#ifndef TALWEG_TYPES_H
#define TALWEG_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TALWEG_MAGIC 0x54414C57U

#define TALWEG_INVALID_PPN  (0xFFFFFFFFU)
#define TALWEG_UNMAPPED_PPN (0xFFFFFFFEU)
#define TALWEG_INVALID_BLOCK (0xFFFFFFFFU)

typedef uint32_t talweg_lba_t;
typedef uint32_t talweg_ppn_t;
typedef uint32_t talweg_block_id_t;
typedef uint16_t talweg_channel_t;
typedef uint16_t talweg_die_t;
typedef uint16_t talweg_plane_t;
typedef uint16_t talweg_page_idx_t;
typedef uint64_t talweg_seq_t;
typedef uint32_t talweg_crc_t;

typedef enum {
    TALWEG_OK = 0,
    TALWEG_ERR_OUT_OF_BOUNDS = -1,
    TALWEG_ERR_MEDIA_IO = -2,
    TALWEG_ERR_NO_FREE_BLOCKS = -3,
    TALWEG_ERR_WRITE_PROTECT = -4,
    TALWEG_ERR_PAGE_ALREADY_WRITTEN = -5,
    TALWEG_ERR_CORRUPT_METADATA = -6,
    TALWEG_ERR_BAD_BLOCK = -7,
    TALWEG_ERR_POWER_LOSS = -8,
    TALWEG_ERR_UNMAPPED = -9,
    TALWEG_ERR_INVAL = -10,
    TALWEG_ERR_NOMEM = -11,
    TALWEG_ERR_NOT_FOUND = -12
} talweg_status_t;

typedef enum {
    TALWEG_BLK_FREE = 0,
    TALWEG_BLK_OPEN = 1,
    TALWEG_BLK_CLOSED = 2,
    TALWEG_BLK_VICTIM = 3,
    TALWEG_BLK_BAD = 4
} talweg_block_state_t;

typedef enum {
    TALWEG_PAGE_FREE = 0,
    TALWEG_PAGE_VALID = 1,
    TALWEG_PAGE_INVALID = 2
} talweg_page_state_t;

typedef enum {
    TALWEG_GC_GREEDY = 0,
    TALWEG_GC_COST_BENEFIT = 1,
    TALWEG_GC_CAT = 2
} talweg_gc_policy_t;

typedef struct {
    uint32_t channels;
    uint32_t dies_per_channel;
    uint32_t planes_per_die;
    uint32_t blocks_per_plane;
    uint32_t pages_per_block;
    uint32_t page_size;
    uint32_t oob_size;
    uint32_t total_blocks;
    uint32_t total_pages;
    uint32_t user_lba_count;
} talweg_geometry_t;

typedef struct {
    talweg_channel_t channel;
    talweg_die_t die;
    talweg_plane_t plane;
    talweg_block_id_t block;
    talweg_page_idx_t page;
} talweg_phys_addr_t;

typedef struct {
    talweg_lba_t lba;
    talweg_seq_t seq;
    uint32_t flags;
    uint32_t crc_data;
    uint32_t crc_header;
} talweg_oob_t;

const char *talweg_status_str(talweg_status_t status);
const char *talweg_block_state_str(talweg_block_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_TYPES_H */
