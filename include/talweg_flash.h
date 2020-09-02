#ifndef TALWEG_FLASH_H
#define TALWEG_FLASH_H

#include "talweg_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t reads;
    uint64_t progs;
    uint64_t erases;
    uint64_t bitflips_injected;
    uint64_t bitflips_corrected;
} talweg_media_stats_t;

typedef struct {
    talweg_geometry_t geom;
    int fd;
    uint8_t *raw_media;
    size_t raw_size;
    size_t block_bytes;
    size_t page_total_bytes;
    talweg_media_stats_t stats;
    bool enforce_sequential_prog;
    bool simulate_latencies;
} talweg_flash_dev_t;

int talweg_media_create(const char *path, const talweg_geometry_t *geom);
int talweg_media_open(talweg_flash_dev_t **out_dev, const char *path, const talweg_geometry_t *geom);
void talweg_media_close(talweg_flash_dev_t *dev);

int talweg_media_read_page(talweg_flash_dev_t *dev, uint32_t ppn, void *buf, talweg_oob_t *oob);
int talweg_media_prog_page(talweg_flash_dev_t *dev, uint32_t ppn, const void *buf, const talweg_oob_t *oob);
int talweg_media_erase_block(talweg_flash_dev_t *dev, uint32_t blk);
int talweg_media_sync(talweg_flash_dev_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_FLASH_H */
