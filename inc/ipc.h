#ifndef IPC_H
#define IPC_H

#include <stddef.h>

typedef enum {
    IPC_TYPE_FIFO,
    IPC_TYPE_MSGQUEUE,
    IPC_TYPE_SHM,
    IPC_TYPE_UDS,
    IPC_TYPE_COUNT
} ipc_type_t;

typedef enum {
    IPC_ROLE_SERVER,
    IPC_ROLE_CLIENT
} ipc_role_t;

typedef struct ipc_context ipc_context_t;

struct ipc_ops {
    int (*init)(ipc_context_t *ctx);
    int (*send)(ipc_context_t *ctx, const void *buf, size_t len);
    int (*recv)(ipc_context_t *ctx, void *buf, size_t len);
    void (*cleanup)(ipc_context_t *ctx);
};

struct ipc_context {
    ipc_type_t type;
    ipc_role_t role;
    struct ipc_ops ops;
    void *priv;
};

int ipc_init(ipc_context_t *ctx, ipc_type_t type, ipc_role_t role,
             const char *config);

int ipc_send(ipc_context_t *ctx, const void *buf, size_t len);

int ipc_recv(ipc_context_t *ctx, void *buf, size_t len);

void ipc_cleanup(ipc_context_t *ctx);

const char *ipc_type_name(ipc_type_t type);

#endif
