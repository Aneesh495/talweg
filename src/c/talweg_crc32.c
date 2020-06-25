#include "talweg_crc32.h"
#include <stdbool.h>

#define CRC32C_POLY 0x82F63B78U

static uint32_t s_crc32c_table[8][256];
static bool s_initialized = false;

void talweg_crc32c_init(void) {
    if (s_initialized) {
        return;
    }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? CRC32C_POLY : 0);
        }
        s_crc32c_table[0][i] = crc;
    }
    for (uint32_t i = 0; i < 256; i++) {
        for (int j = 1; j < 8; j++) {
            s_crc32c_table[j][i] = (s_crc32c_table[j - 1][i] >> 8) ^
                                   s_crc32c_table[0][s_crc32c_table[j - 1][i] & 0xFF];
        }
    }
    s_initialized = true;
}

uint32_t talweg_crc32c(uint32_t seed, const void *buf, size_t len) {
    if (!s_initialized) {
        talweg_crc32c_init();
    }
    const uint8_t *p = (const uint8_t *)buf;
    uint32_t crc = seed;

    while (len >= 8) {
        crc ^= *(const uint32_t *)p;
        uint32_t next = *(const uint32_t *)(p + 4);
        crc = s_crc32c_table[7][crc & 0xFF] ^
              s_crc32c_table[6][(crc >> 8) & 0xFF] ^
              s_crc32c_table[5][(crc >> 16) & 0xFF] ^
              s_crc32c_table[4][(crc >> 24) & 0xFF] ^
              s_crc32c_table[3][next & 0xFF] ^
              s_crc32c_table[2][(next >> 8) & 0xFF] ^
              s_crc32c_table[1][(next >> 16) & 0xFF] ^
              s_crc32c_table[0][(next >> 24) & 0xFF];
        p += 8;
        len -= 8;
    }

    while (len > 0) {
        crc = (crc >> 8) ^ s_crc32c_table[0][(crc ^ *p) & 0xFF];
        p++;
        len--;
    }

    return crc;
}
