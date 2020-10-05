#include "talweg_client.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

struct talweg_client {
    int fd;
    uint64_t next_tag;
};

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

int talweg_client_connect(talweg_client_t **out_client, const char *socket_path) {
    if (!out_client || !socket_path) return TALWEG_ERR_INVAL;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return TALWEG_ERR_MEDIA_IO;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return TALWEG_ERR_MEDIA_IO;
    }

    talweg_client_t *c = (talweg_client_t *)calloc(1, sizeof(talweg_client_t));
    if (!c) {
        close(fd);
        return TALWEG_ERR_NOMEM;
    }
    c->fd = fd;
    c->next_tag = 1;

    *out_client = c;
    return TALWEG_OK;
}

void talweg_client_disconnect(talweg_client_t *client) {
    if (client) {
        if (client->fd >= 0) close(client->fd);
        free(client);
    }
}

int talweg_client_read(talweg_client_t *client, uint32_t lba, void *buf, size_t buf_size) {
    if (!client || !buf || buf_size < 4096) return TALWEG_ERR_INVAL;

    talweg_proto_hdr_t req;
    memset(&req, 0, sizeof(req));
    req.magic = TALWEG_PROTO_MAGIC;
    req.cmd = TALWEG_CMD_READ;
    req.tag = client->next_tag++;
    req.lba = lba;
    req.count = 1;

    if (full_write(client->fd, &req, sizeof(req)) != (ssize_t)sizeof(req)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    talweg_proto_hdr_t resp;
    if (full_read(client->fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    if (resp.status != TALWEG_OK) {
        return resp.status;
    }

    if (resp.payload_len > 0) {
        if (full_read(client->fd, buf, resp.payload_len) != (ssize_t)resp.payload_len) {
            return TALWEG_ERR_MEDIA_IO;
        }
    }

    return TALWEG_OK;
}

int talweg_client_write(talweg_client_t *client, uint32_t lba, const void *buf, size_t buf_size) {
    if (!client || !buf || buf_size < 4096) return TALWEG_ERR_INVAL;

    talweg_proto_hdr_t req;
    memset(&req, 0, sizeof(req));
    req.magic = TALWEG_PROTO_MAGIC;
    req.cmd = TALWEG_CMD_WRITE;
    req.tag = client->next_tag++;
    req.lba = lba;
    req.count = 1;
    req.payload_len = 4096;

    if (full_write(client->fd, &req, sizeof(req)) != (ssize_t)sizeof(req)) {
        return TALWEG_ERR_MEDIA_IO;
    }
    if (full_write(client->fd, buf, 4096) != 4096) {
        return TALWEG_ERR_MEDIA_IO;
    }

    talweg_proto_hdr_t resp;
    if (full_read(client->fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    return resp.status;
}

int talweg_client_trim(talweg_client_t *client, uint32_t lba) {
    if (!client) return TALWEG_ERR_INVAL;

    talweg_proto_hdr_t req;
    memset(&req, 0, sizeof(req));
    req.magic = TALWEG_PROTO_MAGIC;
    req.cmd = TALWEG_CMD_TRIM;
    req.tag = client->next_tag++;
    req.lba = lba;

    if (full_write(client->fd, &req, sizeof(req)) != (ssize_t)sizeof(req)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    talweg_proto_hdr_t resp;
    if (full_read(client->fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    return resp.status;
}

int talweg_client_flush(talweg_client_t *client) {
    if (!client) return TALWEG_ERR_INVAL;

    talweg_proto_hdr_t req;
    memset(&req, 0, sizeof(req));
    req.magic = TALWEG_PROTO_MAGIC;
    req.cmd = TALWEG_CMD_FLUSH;
    req.tag = client->next_tag++;

    if (full_write(client->fd, &req, sizeof(req)) != (ssize_t)sizeof(req)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    talweg_proto_hdr_t resp;
    if (full_read(client->fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    return resp.status;
}

int talweg_client_gc(talweg_client_t *client) {
    if (!client) return TALWEG_ERR_INVAL;

    talweg_proto_hdr_t req;
    memset(&req, 0, sizeof(req));
    req.magic = TALWEG_PROTO_MAGIC;
    req.cmd = TALWEG_CMD_GC;
    req.tag = client->next_tag++;

    if (full_write(client->fd, &req, sizeof(req)) != (ssize_t)sizeof(req)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    talweg_proto_hdr_t resp;
    if (full_read(client->fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    return resp.status;
}

int talweg_client_stats(talweg_client_t *client, talweg_proto_stats_payload_t *out_stats) {
    if (!client || !out_stats) return TALWEG_ERR_INVAL;

    talweg_proto_hdr_t req;
    memset(&req, 0, sizeof(req));
    req.magic = TALWEG_PROTO_MAGIC;
    req.cmd = TALWEG_CMD_STATS;
    req.tag = client->next_tag++;

    if (full_write(client->fd, &req, sizeof(req)) != (ssize_t)sizeof(req)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    talweg_proto_hdr_t resp;
    if (full_read(client->fd, &resp, sizeof(resp)) != (ssize_t)sizeof(resp)) {
        return TALWEG_ERR_MEDIA_IO;
    }

    if (resp.status != TALWEG_OK) return resp.status;

    if (resp.payload_len == sizeof(talweg_proto_stats_payload_t)) {
        if (full_read(client->fd, out_stats, sizeof(talweg_proto_stats_payload_t)) != (ssize_t)sizeof(talweg_proto_stats_payload_t)) {
            return TALWEG_ERR_MEDIA_IO;
        }
    }

    return TALWEG_OK;
}
