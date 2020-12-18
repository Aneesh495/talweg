#include "talweg_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_format(int argc, char **argv) {
    const char *out_path = "storage.flash";
    uint32_t total_blocks = 256;
    uint32_t ppb = 64;
    uint32_t ps = 4096;
    uint32_t oob = 128;
    uint32_t channels = 4;
    uint32_t dies = 2;
    uint32_t planes = 2;

    uint32_t user_lba = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--blocks") == 0 && i + 1 < argc) {
            total_blocks = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--pages") == 0 && i + 1 < argc) {
            ppb = (uint32_t)atoi(argv[++i]);
        } else if (strcmp(argv[i], "--user-lba") == 0 && i + 1 < argc) {
            user_lba = (uint32_t)atoi(argv[++i]);
        }
    }

    talweg_geometry_t geom;
    memset(&geom, 0, sizeof(geom));
    geom.channels = channels;
    geom.dies_per_channel = dies;
    geom.planes_per_die = planes;
    geom.blocks_per_plane = total_blocks / (channels * dies * planes);
    if (geom.blocks_per_plane == 0) geom.blocks_per_plane = 16;
    geom.total_blocks = total_blocks;
    geom.pages_per_block = ppb;
    geom.page_size = ps;
    geom.oob_size = oob;
    geom.total_pages = total_blocks * ppb;
    geom.user_lba_count = user_lba > 0 ? user_lba : ((total_blocks * 7 / 10) * ppb);

    printf("====================================================\n");
    printf(" Talweg Flash Media Formatter\n");
    printf("====================================================\n");
    printf("Target File:        %s\n", out_path);
    printf("Total Flash Blocks: %u\n", geom.total_blocks);
    printf("Pages per Block:    %u\n", geom.pages_per_block);
    printf("User Page Size:     %u bytes\n", geom.page_size);
    printf("OOB Spare Size:     %u bytes\n", geom.oob_size);
    printf("Total Raw Capacity: %.2f MB\n", (double)(geom.total_pages * (ps + oob)) / (1024.0 * 1024.0));
    printf("Usable User LBA:    %u sectors (%.2f MB)\n", geom.user_lba_count, (double)(geom.user_lba_count * ps) / (1024.0 * 1024.0));
    printf("Overprovisioning:   30.0%%\n");
    printf("====================================================\n");

    int rc = talweg_media_create(out_path, &geom);
    if (rc == TALWEG_OK) {
        printf("Success: Media image formatted successfully to %s\n", out_path);
        return 0;
    } else {
        fprintf(stderr, "Error: Failed to format media image (status %d: %s)\n", rc, talweg_status_str(rc));
        return 1;
    }
}
