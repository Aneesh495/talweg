#include "talweg_types.h"

const char *talweg_status_str(talweg_status_t status) {
    switch (status) {
        case TALWEG_OK: return "OK";
        case TALWEG_ERR_OUT_OF_BOUNDS: return "OUT_OF_BOUNDS";
        case TALWEG_ERR_MEDIA_IO: return "MEDIA_IO";
        case TALWEG_ERR_NO_FREE_BLOCKS: return "NO_FREE_BLOCKS";
        case TALWEG_ERR_WRITE_PROTECT: return "WRITE_PROTECT";
        case TALWEG_ERR_PAGE_ALREADY_WRITTEN: return "PAGE_ALREADY_WRITTEN";
        case TALWEG_ERR_CORRUPT_METADATA: return "CORRUPT_METADATA";
        case TALWEG_ERR_BAD_BLOCK: return "BAD_BLOCK";
        case TALWEG_ERR_POWER_LOSS: return "POWER_LOSS";
        case TALWEG_ERR_UNMAPPED: return "UNMAPPED";
        case TALWEG_ERR_INVAL: return "INVALID_ARGUMENT";
        case TALWEG_ERR_NOMEM: return "OUT_OF_MEMORY";
        case TALWEG_ERR_NOT_FOUND: return "NOT_FOUND";
        default: return "UNKNOWN_ERROR";
    }
}

const char *talweg_block_state_str(talweg_block_state_t state) {
    switch (state) {
        case TALWEG_BLK_FREE: return "FREE";
        case TALWEG_BLK_OPEN: return "OPEN";
        case TALWEG_BLK_CLOSED: return "CLOSED";
        case TALWEG_BLK_VICTIM: return "VICTIM";
        case TALWEG_BLK_BAD: return "BAD";
        default: return "UNKNOWN";
    }
}
