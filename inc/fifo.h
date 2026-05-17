#ifndef FIFO_H
#define FIFO_H

#include "ipc.h"

#define FIFO_PATH_TX "/tmp/ipc_fifo_tx"
#define FIFO_PATH_RX "/tmp/ipc_fifo_rx"

int fifo_server_init(ipc_context_t *ctx);
int fifo_client_init(ipc_context_t *ctx);
int fifo_send(ipc_context_t *ctx, const void *buf, size_t len);
int fifo_recv(ipc_context_t *ctx, void *buf, size_t len);
void fifo_cleanup(ipc_context_t *ctx);

#endif
