#include "talweg_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

int cmd_bench(int argc, char **argv) {
    const char *socket_path = "/tmp/talweg.sock";
    const char *workload = "mixed";
    uint32_t total_ops = 5000;
    uint32_t max_lba = 10000;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (strcmp(argv[i], "--workload") == 0 && i + 1 < argc) {
            workload = argv[++i];
        } else if (strcmp(argv[i], "--ops") == 0 && i + 1 < argc) {
            total_ops = (uint32_t)atoi(argv[++i]);
        }
    }

    talweg_client_t *client = NULL;
    int rc = talweg_client_connect(&client, socket_path);
    if (rc != TALWEG_OK) {
        fprintf(stderr, "Error: Could not connect to Talweg daemon at %s\n", socket_path);
        return 1;
    }

    talweg_proto_stats_payload_t initial_stats;
    talweg_client_stats(client, &initial_stats);

    printf("====================================================\n");
    printf(" Talweg High-Throughput FTL Storage Benchmark\n");
    printf("====================================================\n");
    printf("Workload Profile:   %s\n", workload);
    printf("Total Operations:   %u\n", total_ops);
    printf("Page Payload Size:  4096 bytes (4 KB)\n");
    printf("Starting Benchmark...\n");

    uint8_t *page_buf = (uint8_t *)malloc(4096);
    if (!page_buf) {
        talweg_client_disconnect(client);
        return 1;
    }
    memset(page_buf, 0x55, 4096);

    srand(1337);
    double t_start = get_time_sec();
    uint32_t success_ops = 0;

    for (uint32_t op = 0; op < total_ops; op++) {
        uint32_t lba = (uint32_t)(rand() % max_lba);
        bool do_write = true;

        if (strcmp(workload, "randread") == 0) {
            do_write = false;
        } else if (strcmp(workload, "randwrite") == 0) {
            do_write = true;
        } else if (strcmp(workload, "seqwrite") == 0) {
            lba = op % max_lba;
            do_write = true;
        } else {
            /* Mixed 70% writes / 30% reads */
            do_write = (rand() % 100) < 70;
        }

        if (do_write) {
            *(uint32_t *)page_buf = op;
            if (talweg_client_write(client, lba, page_buf, 4096) == TALWEG_OK) {
                success_ops++;
            }
        } else {
            if (talweg_client_read(client, lba, page_buf, 4096) == TALWEG_OK) {
                success_ops++;
            }
        }
    }

    double t_end = get_time_sec();
    double duration = t_end - t_start;
    double iops = (duration > 0.0) ? ((double)success_ops / duration) : 0.0;
    double mbps = (duration > 0.0) ? ((double)(success_ops * 4096) / (duration * 1024.0 * 1024.0)) : 0.0;
    double avg_latency_us = (success_ops > 0) ? ((duration * 1e6) / (double)success_ops) : 0.0;

    talweg_proto_stats_payload_t final_stats;
    talweg_client_stats(client, &final_stats);
    talweg_client_disconnect(client);
    free(page_buf);

    double run_waf = (double)final_stats.waf_mille / 1000.0;

    printf("\nBenchmark Completed in %.4f seconds:\n", duration);
    printf("  Successful I/O Ops: %u / %u\n", success_ops, total_ops);
    printf("  Throughput (IOPS):  %.2f ops/sec\n", iops);
    printf("  Bandwidth:          %.2f MB/sec\n", mbps);
    printf("  Average Latency:    %.2f microseconds\n", avg_latency_us);
    printf("  Write Amplification:%.3f (WAF)\n", run_waf);
    printf("  GC Cycles Triggered:%llu\n", (unsigned long long)(final_stats.gc_cycles - initial_stats.gc_cycles));
    printf("====================================================\n");

    return 0;
}
