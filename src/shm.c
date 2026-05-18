#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <time.h>
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

#define CS_SEM  0
#define SC_SEM  1
#define TIMEOUT_SEC 3

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

    priv->semid = semget(SEM_KEY, 2, IPC_CREAT | 0666);
    if (priv->semid < 0) {
        perror("semget server");
        shmdt(priv->addr);
        shmctl(priv->shmid, IPC_RMID, NULL);
        free(priv);
        return -1;
    }

    {
        union semun arg;
        unsigned short vals[2] = { 0, 0 };
        arg.array = vals;
        semctl(priv->semid, 0, SETALL, arg);
    }

    priv->offset_w = SHM_SIZE / 2;
    priv->offset_r = 0;

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

    priv->semid = semget(SEM_KEY, 2, 0666);
    if (priv->semid < 0) {
        perror("semget client");
        shmdt(priv->addr);
        free(priv);
        return -1;
    }

    priv->offset_w = 0;
    priv->offset_r = SHM_SIZE / 2;

    ctx->priv        = priv;
    ctx->ops.send    = shm_send;
    ctx->ops.recv    = shm_recv;
    ctx->ops.cleanup = shm_cleanup;
    return 0;
}

int shm_send(ipc_context_t *ctx, const void *buf, size_t len)
{
    struct shm_priv *priv = ctx->priv;
    char *base = (char *)priv->addr + priv->offset_w;
    int signal_sem = (ctx->role == IPC_ROLE_CLIENT) ? CS_SEM : SC_SEM;

    uint32_t net_len = htonl((uint32_t)len);
    memcpy(base, &net_len, sizeof(net_len));
    memcpy(base + sizeof(net_len), buf, len);

    struct sembuf sb = { (unsigned short)signal_sem, 1, 0 };
    if (semop(priv->semid, &sb, 1) < 0) {
        perror("semop post");
        return -1;
    }
    return (int)len;
}

int shm_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct shm_priv *priv = ctx->priv;
    char *base = (char *)priv->addr + priv->offset_r;
    int wait_sem = (ctx->role == IPC_ROLE_SERVER) ? CS_SEM : SC_SEM;

    struct sembuf sb = { (unsigned short)wait_sem, -1, 0 };

    if (ctx->role == IPC_ROLE_SERVER) {
        struct timespec ts;
        ts.tv_sec = time(NULL) + TIMEOUT_SEC;
        ts.tv_nsec = 0;
        if (semtimedop(priv->semid, &sb, 1, &ts) < 0) {
            if (errno == EAGAIN) return -1;
            perror("semtimedop");
            return -1;
        }
    } else {
        if (semop(priv->semid, &sb, 1) < 0) {
            perror("semop wait");
            return -1;
        }
    }

    uint32_t net_len;
    memcpy(&net_len, base, sizeof(net_len));
    uint32_t pkt_len = ntohl(net_len);
    if ((size_t)pkt_len > len) return -1;

    memcpy(buf, base + sizeof(net_len), pkt_len);
    memset(base, 0, sizeof(net_len));
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
