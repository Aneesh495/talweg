#include "talweg_ftl.h"
#include <stdio.h>
#include <string.h>

extern int cmd_format(int argc, char **argv);
extern int cmd_inspect(int argc, char **argv);
extern int cmd_heatmap(int argc, char **argv);
extern int cmd_bench(int argc, char **argv);
extern int cmd_crash(int argc, char **argv);
extern int talweg_server_run(talweg_ftl_t *ftl, const char *socket_path);

static void print_usage(const char *prog) {
    printf("Talweg: High-Assurance Flash Translation Layer & Virtual Solid-State Engine\n");
    printf("Usage: %s <subcommand> [options]\n\n", prog);
    printf("Subcommands:\n");
    printf("  format       Format raw flash media storage image\n");
    printf("  serve        Launch Talweg storage daemon server\n");
    printf("  inspect      Query live controller status and wear metrics\n");
    printf("  heatmap      Render ANSI block wear leveling terminal map\n");
    printf("  bench        Run high-throughput synthetic block I/O benchmarks\n");
    printf("  crash-drill  Execute Monte Carlo power-cut recovery verification\n");
    printf("  version      Display version and build metadata\n\n");
    printf("Run '%s <subcommand> --help' for details on a specific command.\n", prog);
}

static int cmd_serve_entry(int argc, char **argv) {
    const char *socket_path = "/tmp/talweg.sock";
    const char *media_path = "storage.flash";

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--socket") == 0 && i + 1 < argc) {
            socket_path = argv[++i];
        } else if (strcmp(argv[i], "--media") == 0 && i + 1 < argc) {
            media_path = argv[++i];
        }
    }

    talweg_geometry_t geom;
    memset(&geom, 0, sizeof(geom));
    geom.channels = 4;
    geom.dies_per_channel = 2;
    geom.planes_per_die = 2;
    geom.blocks_per_plane = 16;
    geom.total_blocks = 256;
    geom.pages_per_block = 64;
    geom.page_size = 4096;
    geom.oob_size = 128;
    geom.total_pages = geom.total_blocks * geom.pages_per_block;
    geom.user_lba_count = 10000;

    talweg_flash_dev_t *dev = NULL;
    int rc = talweg_media_open(&dev, media_path, &geom);
    if (rc != TALWEG_OK) {
        printf("Media file '%s' not found. Creating new flash media...\n", media_path);
        talweg_media_create(media_path, &geom);
        rc = talweg_media_open(&dev, media_path, &geom);
        if (rc != TALWEG_OK) {
            fprintf(stderr, "Fatal: Could not initialize flash media (status %d)\n", rc);
            return 1;
        }
    }

    talweg_ftl_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.geom = geom;
    cfg.gc_policy = TALWEG_GC_COST_BENEFIT;

    talweg_ftl_t *ftl = NULL;
    talweg_ftl_init(&ftl, &cfg);
    talweg_ftl_attach_media(ftl, dev);

    int s_rc = talweg_server_run(ftl, socket_path);

    talweg_ftl_free(ftl);
    talweg_media_close(dev);
    return s_rc;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *subcmd = argv[1];
    int sub_argc = argc - 2;
    char **sub_argv = argv + 2;

    if (strcmp(subcmd, "format") == 0) {
        return cmd_format(sub_argc, sub_argv);
    } else if (strcmp(subcmd, "serve") == 0) {
        return cmd_serve_entry(sub_argc, sub_argv);
    } else if (strcmp(subcmd, "inspect") == 0) {
        return cmd_inspect(sub_argc, sub_argv);
    } else if (strcmp(subcmd, "heatmap") == 0) {
        return cmd_heatmap(sub_argc, sub_argv);
    } else if (strcmp(subcmd, "bench") == 0) {
        return cmd_bench(sub_argc, sub_argv);
    } else if (strcmp(subcmd, "crash-drill") == 0) {
        return cmd_crash(sub_argc, sub_argv);
    } else if (strcmp(subcmd, "version") == 0 || strcmp(subcmd, "-v") == 0 || strcmp(subcmd, "--version") == 0) {
        printf("talweg v1.0.0 (ATS2-Postiats / C17 Core)\n");
        printf("High-Assurance Solid-State Storage Controller & FTL Firmware Engine\n");
        return 0;
    } else {
        fprintf(stderr, "Unknown subcommand: %s\n", subcmd);
        print_usage(argv[0]);
        return 1;
    }
}
