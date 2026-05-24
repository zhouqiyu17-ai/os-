#ifndef MSGQUEUE_H
#define MSGQUEUE_H

#include "ipc.h"

#define MSGQUEUE_KEY 0x12345678
#define MSG_TYPE_BASE 1
#define MSG_MAX_LEN (8192)

struct msgbuf_custom {
    long mtype;
    char mtext[];
};

int msgqueue_server_init(ipc_context_t *ctx);
int msgqueue_client_init(ipc_context_t *ctx);
int msgqueue_send(ipc_context_t *ctx, const void *buf, size_t len);
int msgqueue_recv(ipc_context_t *ctx, void *buf, size_t len);
void msgqueue_cleanup(ipc_context_t *ctx);

#endif
