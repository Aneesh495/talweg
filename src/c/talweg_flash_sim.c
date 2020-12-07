#include "talweg_flash.h"
#include "talweg_crc32.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TALWEG_MEDIA_HEADER_MAGIC 0x54464D44U /* "TFMD" */

typedef struct {
    uint32_t magic;
    uint32_t version;
    talweg_geometry_t geom;
    uint32_t checksum;
} talweg_media_hdr_t;

int talweg_media_create(const char *path, const talweg_geometry_t *geom) {
    if (!path || !geom) return TALWEG_ERR_INVAL;

    size_t page_total = geom->page_size + geom->oob_size;
    size_t block_bytes = geom->pages_per_block * page_total;
    size_t total_bytes = sizeof(talweg_media_hdr_t) + (geom->total_blocks * block_bytes);

    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return TALWEG_ERR_MEDIA_IO;

    /* Write header */
    talweg_media_hdr_t hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = TALWEG_MEDIA_HEADER_MAGIC;
    hdr.version = 1;
    hdr.geom = *geom;
    hdr.checksum = talweg_crc32c_calc(&hdr.geom, sizeof(talweg_geometry_t));

    if (write(fd, &hdr, sizeof(hdr)) != (ssize_t)sizeof(hdr)) {
        close(fd);
        return TALWEG_ERR_MEDIA_IO;
    }

    /* Allocate full media initialized to erased state 0xFF */
    uint8_t *chunk = (uint8_t *)malloc(65536);
    if (!chunk) {
        close(fd);
        return TALWEG_ERR_NOMEM;
    }
    memset(chunk, 0xFF, 65536);

    size_t remaining = total_bytes - sizeof(hdr);
    while (remaining > 0) {
        size_t to_write = (remaining < 65536) ? remaining : 65536;
        if (write(fd, chunk, to_write) != (ssize_t)to_write) {
            free(chunk);
            close(fd);
            return TALWEG_ERR_MEDIA_IO;
        }
        remaining -= to_write;
    }
    free(chunk);
    close(fd);
    return TALWEG_OK;
}

int talweg_media_open(talweg_flash_dev_t **out_dev, const char *path, const talweg_geometry_t *geom) {
    (void)geom;
    if (!out_dev || !path) return TALWEG_ERR_INVAL;

    int fd = open(path, O_RDWR);
    if (fd < 0) return TALWEG_ERR_MEDIA_IO;

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size < (off_t)sizeof(talweg_media_hdr_t)) {
        close(fd);
        return TALWEG_ERR_MEDIA_IO;
    }

    void *map = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        return TALWEG_ERR_MEDIA_IO;
    }

    talweg_media_hdr_t *hdr = (talweg_media_hdr_t *)map;
    if (hdr->magic != TALWEG_MEDIA_HEADER_MAGIC) {
        munmap(map, st.st_size);
        close(fd);
        return TALWEG_ERR_CORRUPT_METADATA;
    }

    talweg_flash_dev_t *dev = (talweg_flash_dev_t *)calloc(1, sizeof(talweg_flash_dev_t));
    if (!dev) {
        munmap(map, st.st_size);
        close(fd);
        return TALWEG_ERR_NOMEM;
    }

    dev->geom = hdr->geom;
    dev->fd = fd;
    dev->raw_media = (uint8_t *)map + sizeof(talweg_media_hdr_t);
    dev->raw_size = st.st_size;
    dev->page_total_bytes = dev->geom.page_size + dev->geom.oob_size;
    dev->block_bytes = dev->geom.pages_per_block * dev->page_total_bytes;
    dev->enforce_sequential_prog = true;
    dev->simulate_latencies = false;

    *out_dev = dev;
    return TALWEG_OK;
}

void talweg_media_close(talweg_flash_dev_t *dev) {
    if (dev) {
        if (dev->raw_media) {
            void *base = dev->raw_media - sizeof(talweg_media_hdr_t);
            msync(base, dev->raw_size, MS_SYNC);
            munmap(base, dev->raw_size);
        }
        if (dev->fd >= 0) {
            close(dev->fd);
        }
        free(dev);
    }
}

int talweg_media_read_page(talweg_flash_dev_t *dev, uint32_t ppn, void *buf, talweg_oob_t *oob) {
    if (!dev || !buf) return TALWEG_ERR_INVAL;
    if (ppn >= dev->geom.total_pages) return TALWEG_ERR_OUT_OF_BOUNDS;

    dev->stats.reads++;

    size_t offset = (size_t)ppn * dev->page_total_bytes;
    const uint8_t *src_data = dev->raw_media + offset;
    const uint8_t *src_oob = src_data + dev->geom.page_size;

    memcpy(buf, src_data, dev->geom.page_size);
    if (oob) {
        memcpy(oob, src_oob, sizeof(talweg_oob_t));
    }

    return TALWEG_OK;
}

int talweg_media_prog_page(talweg_flash_dev_t *dev, uint32_t ppn, const void *buf, const talweg_oob_t *oob) {
    if (!dev || !buf) return TALWEG_ERR_INVAL;
    if (ppn >= dev->geom.total_pages) return TALWEG_ERR_OUT_OF_BOUNDS;

    size_t offset = (size_t)ppn * dev->page_total_bytes;
    uint8_t *dst_data = dev->raw_media + offset;
    uint8_t *dst_oob = dst_data + dev->geom.page_size;

    /* Verify erase state: in physical flash, program can only turn 1 into 0 */
    if (dev->enforce_sequential_prog) {
        for (size_t i = 0; i < dev->geom.page_size; i++) {
            if (dst_data[i] != 0xFF) {
                return TALWEG_ERR_PAGE_ALREADY_WRITTEN;
            }
        }
    }

    memcpy(dst_data, buf, dev->geom.page_size);
    if (oob) {
        talweg_oob_t cooked = *oob;
        cooked.crc_data = talweg_crc32c_calc(buf, dev->geom.page_size);
        cooked.crc_header = talweg_crc32c_calc(&cooked, sizeof(talweg_oob_t) - sizeof(uint32_t));
        memcpy(dst_oob, &cooked, sizeof(talweg_oob_t));
    } else {
        memset(dst_oob, 0, sizeof(talweg_oob_t));
    }

    dev->stats.progs++;
    return TALWEG_OK;
}

int talweg_media_erase_block(talweg_flash_dev_t *dev, uint32_t blk) {
    if (!dev) return TALWEG_ERR_INVAL;
    if (blk >= dev->geom.total_blocks) return TALWEG_ERR_OUT_OF_BOUNDS;

    size_t offset = (size_t)blk * dev->block_bytes;
    uint8_t *dst = dev->raw_media + offset;
    memset(dst, 0xFF, dev->block_bytes);

    dev->stats.erases++;
    return TALWEG_OK;
}

int talweg_media_sync(talweg_flash_dev_t *dev) {
    if (!dev || !dev->raw_media) return TALWEG_ERR_INVAL;
    void *base = dev->raw_media - sizeof(talweg_media_hdr_t);
    return msync(base, dev->raw_size, MS_SYNC) == 0 ? TALWEG_OK : TALWEG_ERR_MEDIA_IO;
}

int talweg_media_inject_bitflip(talweg_flash_dev_t *dev, uint32_t ppn, uint32_t byte_offset, uint8_t bit_mask) {
    if (!dev || ppn >= dev->geom.total_pages) return TALWEG_ERR_OUT_OF_BOUNDS;
    if (byte_offset >= dev->geom.page_size) return TALWEG_ERR_INVAL;

    size_t offset = (size_t)ppn * dev->page_total_bytes + byte_offset;
    dev->raw_media[offset] ^= bit_mask;
    dev->stats.bitflips_injected++;
    return TALWEG_OK;
}
