#ifndef TALWEG_CRC32_H
#define TALWEG_CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize CRC32C lookup tables (idempotent) */
void talweg_crc32c_init(void);

/* Compute CRC32C checksum over buffer with initial seed */
uint32_t talweg_crc32c(uint32_t seed, const void *buf, size_t len);

/* Convenience wrapper for standard zero-seed computation */
static inline uint32_t talweg_crc32c_calc(const void *buf, size_t len) {
    return talweg_crc32c(0xFFFFFFFFU, buf, len) ^ 0xFFFFFFFFU;
}

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_CRC32_H */
