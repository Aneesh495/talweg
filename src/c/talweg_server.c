#include "talweg_proto.h"
#include "talweg_ftl.h"
#include "talweg_crc32.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

typedef struct {
    talweg_ftl_t *ftl;
    const char *socket_path;
    int port;
    int server_fd;
    volatile bool running;
    pthread_t thread;
} talweg_server_t;

static volatile sig_atomic_t g_stop = 0;

static void handle_sigint(int sig) {
    (void)sig;
    g_stop = 1;
}

static ssize_t full_read(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *p = (char *)buf;
    while (total < count) {
        ssize_t n = read(fd, p + total, count - total);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}

static ssize_t full_write(int fd, const void *buf, size_t count) {
    size_t total = 0;
    const char *p = (const char *)buf;
    while (total < count) {
        ssize_t n = write(fd, p + total, count - total);
        if (n <= 0) return n;
        total += n;
    }
    return (ssize_t)total;
}

static void handle_client(talweg_ftl_t *ftl, int client_fd) {
    talweg_proto_hdr_t req;
    uint8_t *payload_buf = (uint8_t *)malloc(65536);
    if (!payload_buf) {
        close(client_fd);
        return;
    }

    while (!g_stop) {
        ssize_t n = full_read(client_fd, &req, sizeof(req));
        if (n <= 0) break;

        if (req.magic != TALWEG_PROTO_MAGIC) {
            break;
        }

        if (req.payload_len > 0) {
            if (req.payload_len > 65536) break;
            if (full_read(client_fd, payload_buf, req.payload_len) <= 0) break;
        }

        talweg_proto_hdr_t resp;
        memset(&resp, 0, sizeof(resp));
        resp.magic = TALWEG_PROTO_MAGIC;
        resp.tag = req.tag;
        resp.cmd = req.cmd;

        switch (req.cmd) {
            case TALWEG_CMD_PING:
                resp.status = TALWEG_OK;
                resp.payload_len = 0;
                full_write(client_fd, &resp, sizeof(resp));
                break;

            case TALWEG_CMD_READ: {
                int rc = talweg_ftl_read(ftl, req.lba, payload_buf);
                resp.status = rc;
                resp.payload_len = (rc == TALWEG_OK) ? 4096 : 0;
                full_write(client_fd, &resp, sizeof(resp));
                if (resp.payload_len > 0) {
                    full_write(client_fd, payload_buf, resp.payload_len);
                }
                break;
            }

            case TALWEG_CMD_WRITE: {
                int rc = talweg_ftl_write(ftl, req.lba, payload_buf);
                resp.status = rc;
                resp.payload_len = 0;
                full_write(client_fd, &resp, sizeof(resp));
                break;
            }

            case TALWEG_CMD_TRIM: {
                int rc = talweg_ftl_trim(ftl, req.lba);
                resp.status = rc;
                resp.payload_len = 0;
                full_write(client_fd, &resp, sizeof(resp));
                break;
            }

            case TALWEG_CMD_FLUSH: {
                int rc = talweg_ftl_flush(ftl);
                resp.status = rc;
                resp.payload_len = 0;
                full_write(client_fd, &resp, sizeof(resp));
                break;
            }

            case TALWEG_CMD_GC: {
                int rc = talweg_ftl_gc_step(ftl);
                resp.status = rc;
                resp.payload_len = 0;
                full_write(client_fd, &resp, sizeof(resp));
                break;
            }

            case TALWEG_CMD_STATS: {
                talweg_ftl_stats_t stats;
                int rc = talweg_ftl_get_stats(ftl, &stats);
                resp.status = rc;
                if (rc == TALWEG_OK) {
                    talweg_proto_stats_payload_t pld;
                    pld.user_writes = stats.user_writes;
                    pld.flash_writes = stats.flash_writes;
                    pld.gc_cycles = stats.gc_cycles;
                    pld.waf_mille = stats.waf_mille;
                    pld.cache_hits = stats.cache_hits;
                    pld.cache_misses = stats.cache_misses;
                    pld.free_blocks = stats.free_blocks;
                    pld.bad_blocks = stats.bad_blocks;
                    pld.min_erase = stats.min_erase;
                    pld.max_erase = stats.max_erase;
                    pld.wear_delta = stats.wear_delta;
                    pld.mapped_lba_count = stats.mapped_lba_count;

                    resp.payload_len = sizeof(pld);
                    full_write(client_fd, &resp, sizeof(resp));
                    full_write(client_fd, &pld, sizeof(pld));
                } else {
                    resp.payload_len = 0;
                    full_write(client_fd, &resp, sizeof(resp));
                }
                break;
            }

            default:
                resp.status = TALWEG_ERR_INVAL;
                resp.payload_len = 0;
                full_write(client_fd, &resp, sizeof(resp));
                break;
        }
    }

    free(payload_buf);
    close(client_fd);
}

int talweg_server_run(talweg_ftl_t *ftl, const char *socket_path) {
    if (!ftl || !socket_path) return TALWEG_ERR_INVAL;

    unlink(socket_path);

    int sfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sfd < 0) {
        perror("socket");
        return TALWEG_ERR_MEDIA_IO;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sfd);
        return TALWEG_ERR_MEDIA_IO;
    }

    if (listen(sfd, 16) < 0) {
        perror("listen");
        close(sfd);
        unlink(socket_path);
        return TALWEG_ERR_MEDIA_IO;
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    printf("Talweg FTL Server listening on %s\n", socket_path);

    while (!g_stop) {
        int client_fd = accept(sfd, NULL, NULL);
        if (client_fd < 0) {
            if (g_stop) break;
            continue;
        }
        handle_client(ftl, client_fd);
    }

    close(sfd);
    unlink(socket_path);
    printf("Talweg FTL Server shut down cleanly.\n");
    return TALWEG_OK;
}
