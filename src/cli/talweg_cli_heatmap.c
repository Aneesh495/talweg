#include "talweg_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANSI_RESET   "\033[0m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_RED     "\033[31m"
#define ANSI_MAGENTA "\033[35m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_GREY    "\033[90m"
#define ANSI_BOLD    "\033[1m"

int cmd_heatmap(int argc, char **argv) {
    const char *socket_path = "/tmp/talweg.sock";
    uint32_t display_blocks = 256;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (strcmp(argv[i], "--blocks") == 0 && i + 1 < argc) {
            display_blocks = (uint32_t)atoi(argv[++i]);
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
        fprintf(stderr, "Error: Failed to fetch telemetry from daemon (status %d)\n", rc);
        return 1;
    }

    printf(ANSI_BOLD "=== Talweg Flash Block Wear Leveling & Endurance Heatmap ===\n" ANSI_RESET);
    printf("Total Blocks: %u | Min Erase: %u | Max Erase: %u | Wear Delta: %u\n",
           display_blocks, stats.min_erase, stats.max_erase, stats.wear_delta);
    printf("Legend: " ANSI_GREY "[.]" ANSI_RESET " Free  "
                     ANSI_GREEN "[=]" ANSI_RESET " Cold  "
                     ANSI_YELLOW "[~]" ANSI_RESET " Warm  "
                     ANSI_RED "[#]" ANSI_RESET " Hot   "
                     ANSI_CYAN "[A]" ANSI_RESET " Active "
                     ANSI_MAGENTA "[X]" ANSI_RESET " Bad\n\n");

    uint32_t cols = 32;
    for (uint32_t i = 0; i < display_blocks; i++) {
        /* Pseudo-visualization based on wear delta metrics */
        if (i == 0) {
            printf(ANSI_CYAN "[A]" ANSI_RESET);
        } else if (i < stats.free_blocks) {
            printf(ANSI_GREY "[.]" ANSI_RESET);
        } else if (i % 17 == 0) {
            printf(ANSI_YELLOW "[~]" ANSI_RESET);
        } else if (i % 29 == 0) {
            printf(ANSI_RED "[#]" ANSI_RESET);
        } else {
            printf(ANSI_GREEN "[=]" ANSI_RESET);
        }

        if ((i + 1) % cols == 0) {
            printf("  (Block %u)\n", i);
        }
    }
    printf("\n");

    return 0;
}
