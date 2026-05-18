#ifndef SHM_H
#define SHM_H

#include "ipc.h"

#define SHM_KEY    0x22345678
#define SHM_SIZE   (4 * 1024 * 1024)
#define SEM_KEY    0x32345678

struct shm_priv {
    int    shmid;
    void  *addr;
    int   semid;
    size_t offset_w;
    size_t offset_r;
};

int shm_server_init(ipc_context_t *ctx);
int shm_client_init(ipc_context_t *ctx);
int shm_send(ipc_context_t *ctx, const void *buf, size_t len);
int shm_recv(ipc_context_t *ctx, void *buf, size_t len);
void shm_cleanup(ipc_context_t *ctx);

#endif
