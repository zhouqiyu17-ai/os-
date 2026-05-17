#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ipc.h"
#include "fifo.h"
#include "msgqueue.h"
#include "shm.h"
#include "uds.h"

int ipc_init(ipc_context_t *ctx, ipc_type_t type, ipc_role_t role,
             const char *config)
{
    (void)config;
    if (!ctx) return -1;

    memset(ctx, 0, sizeof(*ctx));
    ctx->type = type;
    ctx->role = role;

    switch (type) {
    case IPC_TYPE_FIFO:
        if (role == IPC_ROLE_SERVER)
            return fifo_server_init(ctx);
        else
            return fifo_client_init(ctx);
    case IPC_TYPE_MSGQUEUE:
        if (role == IPC_ROLE_SERVER)
            return msgqueue_server_init(ctx);
        else
            return msgqueue_client_init(ctx);
    case IPC_TYPE_SHM:
        if (role == IPC_ROLE_SERVER)
            return shm_server_init(ctx);
        else
            return shm_client_init(ctx);
    case IPC_TYPE_UDS:
        if (role == IPC_ROLE_SERVER)
            return uds_server_init(ctx);
        else
            return uds_client_init(ctx);
    default:
        return -1;
    }
}

int ipc_send(ipc_context_t *ctx, const void *buf, size_t len)
{
    if (!ctx || !ctx->ops.send) return -1;
    return ctx->ops.send(ctx, buf, len);
}

int ipc_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    if (!ctx || !ctx->ops.recv) return -1;
    return ctx->ops.recv(ctx, buf, len);
}

void ipc_cleanup(ipc_context_t *ctx)
{
    if (!ctx || !ctx->ops.cleanup) return;
    ctx->ops.cleanup(ctx);
}

const char *ipc_type_name(ipc_type_t type)
{
    static const char *names[] = { "FIFO", "MSGQUEUE", "SHM", "UDS" };
    if (type < IPC_TYPE_COUNT) return names[type];
    return "UNKNOWN";
}
