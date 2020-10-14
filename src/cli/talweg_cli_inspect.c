#include "talweg_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cmd_inspect(int argc, char **argv) {
    const char *socket_path = "/tmp/talweg.sock";

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            socket_path = argv[++i];
        }
    }

    talweg_client_t *client = NULL;
    int rc = talweg_client_connect(&client, socket_path);
    if (rc != TALWEG_OK) {
        fprintf(stderr, "Error: Could not connect to Talweg daemon at %s\n", socket_path);
        return 1;
    }

    talweg_proto_stats_payload_t stats;
    rc = talweg_client_stats(client, &stats);
    talweg_client_disconnect(client);

    if (rc != TALWEG_OK) {
        fprintf(stderr, "Error: Failed to query runtime stats (status %d)\n", rc);
        return 1;
    }

    double waf = (double)stats.waf_mille / 1000.0;
    uint64_t total_cache = stats.cache_hits + stats.cache_misses;
    double hit_rate = (total_cache > 0) ? (100.0 * (double)stats.cache_hits / (double)total_cache) : 0.0;

    printf("====================================================\n");
    printf(" Talweg FTL Runtime Telemetry & Controller Status\n");
    printf("====================================================\n");
    printf("Logical Block Allocations:\n");
    printf("  Mapped User LBAs:    %u\n", stats.mapped_lba_count);
    printf("  Free Flash Blocks:   %u\n", stats.free_blocks);
    printf("  Retired Bad Blocks:  %u\n", stats.bad_blocks);
    printf("\nEndurance & Wear Leveling:\n");
    printf("  Minimum Block Erase: %u\n", stats.min_erase);
    printf("  Maximum Block Erase: %u\n", stats.max_erase);
    printf("  Wear Delta (Max-Min):%u\n", stats.wear_delta);
    printf("\nThroughput & Amplification:\n");
    printf("  User Write Requests: %llu\n", (unsigned long long)stats.user_writes);
    printf("  Flash Page Programs: %llu\n", (unsigned long long)stats.flash_writes);
    printf("  GC Reclamation Runs: %llu\n", (unsigned long long)stats.gc_cycles);
    printf("  Write Amplification: %.3f (WAF)\n", waf);
    printf("\nPage Buffer Cache Pool:\n");
    printf("  Cache Hits:          %llu\n", (unsigned long long)stats.cache_hits);
    printf("  Cache Misses:        %llu\n", (unsigned long long)stats.cache_misses);
    printf("  Cache Hit Ratio:     %.2f%%\n", hit_rate);
    printf("====================================================\n");

    return 0;
}
