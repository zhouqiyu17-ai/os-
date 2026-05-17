#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include "shm.h"

#if !defined(__APPLE__)
union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};
#endif

static int sem_lock(int semid)
{
    struct sembuf sb = { 0, -1, 0 };
    return semop(semid, &sb, 1);
}

static int sem_unlock(int semid)
{
    struct sembuf sb = { 0, 1, 0 };
    return semop(semid, &sb, 1);
}

static int sem_init(int semid, int val)
{
    union semun arg;
    arg.val = val;
    return semctl(semid, 0, SETVAL, arg);
}

int shm_server_init(ipc_context_t *ctx)
{
    struct shm_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc shm_priv");
        return -1;
    }

    priv->shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (priv->shmid < 0) {
        perror("shmget server");
        free(priv);
        return -1;
    }

    priv->addr = shmat(priv->shmid, NULL, 0);
    if (priv->addr == (void *)-1) {
        perror("shmat server");
        shmctl(priv->shmid, IPC_RMID, NULL);
        free(priv);
        return -1;
    }

    priv->semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (priv->semid < 0) {
        perror("semget server");
        shmdt(priv->addr);
        shmctl(priv->shmid, IPC_RMID, NULL);
        free(priv);
        return -1;
    }

    sem_init(priv->semid, 1);

    ctx->priv        = priv;
    ctx->ops.send    = shm_send;
    ctx->ops.recv    = shm_recv;
    ctx->ops.cleanup = shm_cleanup;
    return 0;
}

int shm_client_init(ipc_context_t *ctx)
{
    struct shm_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc shm_priv");
        return -1;
    }

    priv->shmid = shmget(SHM_KEY, SHM_SIZE, 0666);
    if (priv->shmid < 0) {
        perror("shmget client");
        free(priv);
        return -1;
    }

    priv->addr = shmat(priv->shmid, NULL, 0);
    if (priv->addr == (void *)-1) {
        perror("shmat client");
        free(priv);
        return -1;
    }

    priv->semid = semget(SEM_KEY, 1, 0666);
    if (priv->semid < 0) {
        perror("semget client");
        shmdt(priv->addr);
        free(priv);
        return -1;
    }

    ctx->priv        = priv;
    ctx->ops.send    = shm_send;
    ctx->ops.recv    = shm_recv;
    ctx->ops.cleanup = shm_cleanup;
    return 0;
}

int shm_send(ipc_context_t *ctx, const void *buf, size_t len)
{
    struct shm_priv *priv = ctx->priv;

    sem_lock(priv->semid);

    uint32_t net_len = htonl((uint32_t)len);
    memcpy((char *)priv->addr, &net_len, sizeof(net_len));
    memcpy((char *)priv->addr + sizeof(net_len), buf, len);

    sem_unlock(priv->semid);
    return (int)len;
}

int shm_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct shm_priv *priv = ctx->priv;

    sem_lock(priv->semid);

    uint32_t net_len;
    memcpy(&net_len, priv->addr, sizeof(net_len));
    uint32_t pkt_len = ntohl(net_len);
    if (pkt_len > len) {
        sem_unlock(priv->semid);
        return -1;
    }

    memcpy(buf, (char *)priv->addr + sizeof(net_len), pkt_len);

    sem_unlock(priv->semid);
    return (int)pkt_len;
}

void shm_cleanup(ipc_context_t *ctx)
{
    struct shm_priv *priv = ctx->priv;
    if (!priv) return;

    if (priv->addr && priv->addr != (void *)-1)
        shmdt(priv->addr);

    if (ctx->role == IPC_ROLE_SERVER) {
        if (priv->shmid >= 0)
            shmctl(priv->shmid, IPC_RMID, NULL);
        if (priv->semid >= 0)
            semctl(priv->semid, 0, IPC_RMID);
    }

    free(priv);
    ctx->priv = NULL;
}
