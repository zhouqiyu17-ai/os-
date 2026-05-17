#ifndef UDS_H
#define UDS_H

#include "ipc.h"

#define UDS_PATH "/tmp/ipc_uds.sock"

int uds_server_init(ipc_context_t *ctx);
int uds_client_init(ipc_context_t *ctx);
int uds_send(ipc_context_t *ctx, const void *buf, size_t len);
int uds_recv(ipc_context_t *ctx, void *buf, size_t len);
void uds_cleanup(ipc_context_t *ctx);

#endif
