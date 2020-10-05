#ifndef TALWEG_CLIENT_H
#define TALWEG_CLIENT_H

#include "talweg_proto.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct talweg_client talweg_client_t;

int talweg_client_connect(talweg_client_t **out_client, const char *socket_path);
void talweg_client_disconnect(talweg_client_t *client);

int talweg_client_read(talweg_client_t *client, uint32_t lba, void *buf, size_t buf_size);
int talweg_client_write(talweg_client_t *client, uint32_t lba, const void *buf, size_t buf_size);
int talweg_client_trim(talweg_client_t *client, uint32_t lba);
int talweg_client_flush(talweg_client_t *client);
int talweg_client_gc(talweg_client_t *client);
int talweg_client_stats(talweg_client_t *client, talweg_proto_stats_payload_t *out_stats);

#ifdef __cplusplus
}
#endif

#endif /* TALWEG_CLIENT_H */
