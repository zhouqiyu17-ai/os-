#!/bin/sh
# 一键安装脚本 - 在 WebVM 中运行: sh install.sh
set -e

mkdir -p ipc_lab/inc ipc_lab/src ipc_lab/test

# ==================== inc/ipc.h ====================
cat > ipc_lab/inc/ipc.h << 'EOF'
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
EOF

# ==================== inc/fifo.h ====================
cat > ipc_lab/inc/fifo.h << 'EOF'
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
EOF

# ==================== inc/msgqueue.h ====================
cat > ipc_lab/inc/msgqueue.h << 'EOF'
#ifndef MSGQUEUE_H
#define MSGQUEUE_H

#include "ipc.h"

#define MSGQUEUE_KEY 0x12345678
#define MSG_TYPE_BASE 1
#define MSG_MAX_LEN 8192

struct msgbuf_custom {
    long mtype;
    char mtext[MSG_MAX_LEN];
};

int msgqueue_server_init(ipc_context_t *ctx);
int msgqueue_client_init(ipc_context_t *ctx);
int msgqueue_send(ipc_context_t *ctx, const void *buf, size_t len);
int msgqueue_recv(ipc_context_t *ctx, void *buf, size_t len);
void msgqueue_cleanup(ipc_context_t *ctx);

#endif
EOF

# ==================== inc/shm.h ====================
cat > ipc_lab/inc/shm.h << 'EOF'
#ifndef SHM_H
#define SHM_H

#include "ipc.h"

#define SHM_KEY    0x22345678
#define SHM_SIZE   (2 * 1024 * 1024)
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
EOF

# ==================== inc/uds.h ====================
cat > ipc_lab/inc/uds.h << 'EOF'
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
EOF

# ==================== src/ipc.c ====================
cat > ipc_lab/src/ipc.c << 'EOF'
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
EOF

# ==================== src/fifo.c ====================
cat > ipc_lab/src/fifo.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "fifo.h"

struct fifo_priv {
    int fd_tx;
    int fd_rx;
};

int fifo_server_init(ipc_context_t *ctx)
{
    struct fifo_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc fifo_priv");
        return -1;
    }

    unlink(FIFO_PATH_TX);
    unlink(FIFO_PATH_RX);

    if (mkfifo(FIFO_PATH_TX, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo tx");
        free(priv);
        return -1;
    }
    if (mkfifo(FIFO_PATH_RX, 0666) < 0 && errno != EEXIST) {
        perror("mkfifo rx");
        unlink(FIFO_PATH_TX);
        free(priv);
        return -1;
    }

    priv->fd_tx = open(FIFO_PATH_TX, O_WRONLY);
    if (priv->fd_tx < 0) {
        perror("open fifo_tx for write");
        unlink(FIFO_PATH_TX);
        unlink(FIFO_PATH_RX);
        free(priv);
        return -1;
    }

    priv->fd_rx = open(FIFO_PATH_RX, O_RDONLY);
    if (priv->fd_rx < 0) {
        perror("open fifo_rx for read");
        close(priv->fd_tx);
        unlink(FIFO_PATH_TX);
        unlink(FIFO_PATH_RX);
        free(priv);
        return -1;
    }

    ctx->priv        = priv;
    ctx->ops.send    = fifo_send;
    ctx->ops.recv    = fifo_recv;
    ctx->ops.cleanup = fifo_cleanup;
    return 0;
}

int fifo_client_init(ipc_context_t *ctx)
{
    struct fifo_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc fifo_priv");
        return -1;
    }

    priv->fd_rx = open(FIFO_PATH_TX, O_RDONLY);
    if (priv->fd_rx < 0) {
        perror("open fifo_tx for read (client)");
        free(priv);
        return -1;
    }

    priv->fd_tx = open(FIFO_PATH_RX, O_WRONLY);
    if (priv->fd_tx < 0) {
        perror("open fifo_rx for write (client)");
        close(priv->fd_rx);
        free(priv);
        return -1;
    }

    ctx->priv        = priv;
    ctx->ops.send    = fifo_send;
    ctx->ops.recv    = fifo_recv;
    ctx->ops.cleanup = fifo_cleanup;
    return 0;
}

int fifo_send(ipc_context_t *ctx, const void *buf, size_t len)
{
    struct fifo_priv *priv = ctx->priv;

    uint32_t net_len = htonl((uint32_t)len);
    if (write(priv->fd_tx, &net_len, sizeof(net_len)) != sizeof(net_len))
        return -1;

    size_t total = 0;
    while (total < len) {
        ssize_t n = write(priv->fd_tx, (const char *)buf + total, len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return (int)total;
}

int fifo_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct fifo_priv *priv = ctx->priv;

    uint32_t net_len;
    if (read(priv->fd_rx, &net_len, sizeof(net_len)) != sizeof(net_len))
        return -1;

    uint32_t pkt_len = ntohl(net_len);
    if (pkt_len > len) return -1;

    size_t total = 0;
    while (total < pkt_len) {
        ssize_t n = read(priv->fd_rx, (char *)buf + total, pkt_len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return (int)total;
}

void fifo_cleanup(ipc_context_t *ctx)
{
    struct fifo_priv *priv = ctx->priv;
    if (!priv) return;

    if (priv->fd_tx >= 0) close(priv->fd_tx);
    if (priv->fd_rx >= 0) close(priv->fd_rx);

    if (ctx->role == IPC_ROLE_SERVER) {
        unlink(FIFO_PATH_TX);
        unlink(FIFO_PATH_RX);
    }

    free(priv);
    ctx->priv = NULL;
}
EOF

# ==================== src/msgqueue.c ====================
cat > ipc_lab/src/msgqueue.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>
#include "msgqueue.h"

struct msgqueue_priv {
    int msgqid;
    pid_t peer_pid;
};

int msgqueue_server_init(ipc_context_t *ctx)
{
    struct msgqueue_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc msgqueue_priv");
        return -1;
    }

    priv->msgqid = msgget(MSGQUEUE_KEY, IPC_CREAT | 0666);
    if (priv->msgqid < 0) {
        perror("msgget server");
        free(priv);
        return -1;
    }

    ctx->priv        = priv;
    ctx->ops.send    = msgqueue_send;
    ctx->ops.recv    = msgqueue_recv;
    ctx->ops.cleanup = msgqueue_cleanup;
    return 0;
}

int msgqueue_client_init(ipc_context_t *ctx)
{
    struct msgqueue_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc msgqueue_priv");
        return -1;
    }

    priv->msgqid = msgget(MSGQUEUE_KEY, 0666);
    if (priv->msgqid < 0) {
        perror("msgget client");
        free(priv);
        return -1;
    }

    ctx->priv        = priv;
    ctx->ops.send    = msgqueue_send;
    ctx->ops.recv    = msgqueue_recv;
    ctx->ops.cleanup = msgqueue_cleanup;
    return 0;
}

int msgqueue_send(ipc_context_t *ctx, const void *buf, size_t len)
{
    struct msgqueue_priv *priv = ctx->priv;
    if (len > MSG_MAX_LEN) {
        fprintf(stderr, "msgqueue_send: data too large\n");
        return -1;
    }

    struct msgbuf_custom msg;
    memset(&msg, 0, sizeof(msg));
    msg.mtype = ctx->role == IPC_ROLE_CLIENT ? MSG_TYPE_BASE : priv->peer_pid;
    memcpy(msg.mtext, buf, len);

    if (msgsnd(priv->msgqid, &msg, len, 0) < 0) {
        perror("msgsnd");
        return -1;
    }
    return (int)len;
}

int msgqueue_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct msgqueue_priv *priv = ctx->priv;
    long recv_type = ctx->role == IPC_ROLE_SERVER ? MSG_TYPE_BASE : (long)getpid();

    if (len > MSG_MAX_LEN) return -1;

    struct msgbuf_custom msg;
    memset(&msg, 0, sizeof(msg));
    ssize_t n = msgrcv(priv->msgqid, &msg, MSG_MAX_LEN, recv_type, 0);
    if (n < 0) {
        if (errno == EINTR) return -1;
        perror("msgrcv");
        return -1;
    }

    if (ctx->role == IPC_ROLE_SERVER)
        priv->peer_pid = (pid_t)atoi(msg.mtext);

    memcpy(buf, msg.mtext, (size_t)n);
    return (int)n;
}

void msgqueue_cleanup(ipc_context_t *ctx)
{
    struct msgqueue_priv *priv = ctx->priv;
    if (!priv) return;

    if (ctx->role == IPC_ROLE_SERVER && priv->msgqid >= 0)
        msgctl(priv->msgqid, IPC_RMID, NULL);

    free(priv);
    ctx->priv = NULL;
}
EOF

# ==================== src/shm.c ====================
cat > ipc_lab/src/shm.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include "shm.h"

union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};

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
EOF

# ==================== src/uds.c ====================
cat > ipc_lab/src/uds.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "uds.h"

struct uds_priv {
    int listen_fd;
    int conn_fd;
};

int uds_server_init(ipc_context_t *ctx)
{
    struct uds_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc uds_priv");
        return -1;
    }

    priv->listen_fd = -1;
    priv->conn_fd   = -1;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket uds");
        free(priv);
        return -1;
    }
    priv->listen_fd = fd;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UDS_PATH, sizeof(addr.sun_path) - 1);

    unlink(UDS_PATH);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind uds");
        close(fd);
        free(priv);
        return -1;
    }

    if (listen(fd, 1) < 0) {
        perror("listen uds");
        unlink(UDS_PATH);
        close(fd);
        free(priv);
        return -1;
    }

    int conn_fd = accept(fd, NULL, NULL);
    if (conn_fd < 0) {
        perror("accept uds");
        unlink(UDS_PATH);
        close(fd);
        free(priv);
        return -1;
    }
    priv->conn_fd = conn_fd;

    ctx->priv        = priv;
    ctx->ops.send    = uds_send;
    ctx->ops.recv    = uds_recv;
    ctx->ops.cleanup = uds_cleanup;
    return 0;
}

int uds_client_init(ipc_context_t *ctx)
{
    struct uds_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc uds_priv");
        return -1;
    }

    priv->listen_fd = -1;
    priv->conn_fd   = socket(AF_UNIX, SOCK_STREAM, 0);
    if (priv->conn_fd < 0) {
        perror("socket uds client");
        free(priv);
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UDS_PATH, sizeof(addr.sun_path) - 1);

    if (connect(priv->conn_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect uds");
        close(priv->conn_fd);
        free(priv);
        return -1;
    }

    ctx->priv        = priv;
    ctx->ops.send    = uds_send;
    ctx->ops.recv    = uds_recv;
    ctx->ops.cleanup = uds_cleanup;
    return 0;
}

int uds_send(ipc_context_t *ctx, const void *buf, size_t len)
{
    struct uds_priv *priv = ctx->priv;
    int fd = priv->conn_fd;

    uint32_t net_len = htonl((uint32_t)len);
    if (write(fd, &net_len, sizeof(net_len)) != sizeof(net_len))
        return -1;

    size_t total = 0;
    while (total < len) {
        ssize_t n = write(fd, (const char *)buf + total, len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return (int)total;
}

int uds_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct uds_priv *priv = ctx->priv;
    int fd = priv->conn_fd;

    uint32_t net_len;
    ssize_t nr = read(fd, &net_len, sizeof(net_len));
    if (nr != sizeof(net_len)) return -1;

    uint32_t pkt_len = ntohl(net_len);
    if (pkt_len > len) return -1;

    size_t total = 0;
    while (total < pkt_len) {
        ssize_t n = read(fd, (char *)buf + total, pkt_len - total);
        if (n <= 0) return -1;
        total += (size_t)n;
    }
    return (int)total;
}

void uds_cleanup(ipc_context_t *ctx)
{
    struct uds_priv *priv = ctx->priv;
    if (!priv) return;

    if (priv->conn_fd >= 0)
        close(priv->conn_fd);

    if (ctx->role == IPC_ROLE_SERVER) {
        if (priv->listen_fd >= 0)
            close(priv->listen_fd);
        unlink(UDS_PATH);
    }

    free(priv);
    ctx->priv = NULL;
}
EOF

# ==================== test/ipc_test.c ====================
cat > ipc_lab/test/ipc_test.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "ipc.h"

static volatile int running = 1;

static void sig_handler(int sig)
{
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <ipc_type> <role> [config]\n", argv[0]);
        fprintf(stderr, "  ipc_type: fifo | msgqueue | shm | uds\n");
        fprintf(stderr, "  role:     server | client\n");
        return 1;
    }

    ipc_type_t type;
    if (strcmp(argv[1], "fifo") == 0)
        type = IPC_TYPE_FIFO;
    else if (strcmp(argv[1], "msgqueue") == 0)
        type = IPC_TYPE_MSGQUEUE;
    else if (strcmp(argv[1], "shm") == 0)
        type = IPC_TYPE_SHM;
    else if (strcmp(argv[1], "uds") == 0)
        type = IPC_TYPE_UDS;
    else {
        fprintf(stderr, "Unknown IPC type: %s\n", argv[1]);
        return 1;
    }

    ipc_role_t role;
    if (strcmp(argv[2], "server") == 0)
        role = IPC_ROLE_SERVER;
    else if (strcmp(argv[2], "client") == 0)
        role = IPC_ROLE_CLIENT;
    else {
        fprintf(stderr, "Unknown role: %s\n", argv[2]);
        return 1;
    }

    ipc_context_t ctx;
    if (ipc_init(&ctx, type, role, NULL) < 0) {
        fprintf(stderr, "IPC init failed\n");
        return 1;
    }

    printf("[%s] %s started, pid=%d\n",
           ipc_type_name(type),
           role == IPC_ROLE_SERVER ? "Server" : "Client",
           getpid());

    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    if (role == IPC_ROLE_SERVER) {
        char buf[4096];
        while (running) {
            memset(buf, 0, sizeof(buf));
            int n = ipc_recv(&ctx, buf, sizeof(buf));
            if (n < 0) {
                if (errno == EINTR) continue;
                perror("server recv");
                break;
            }
            printf("[Server] recv %d bytes: %s\n", n, buf);

            int sent = ipc_send(&ctx, buf, (size_t)n);
            if (sent < 0) {
                perror("server send");
                break;
            }
        }
    } else {
        char send_buf[256];
        int msg_id = 0;
        while (running) {
            snprintf(send_buf, sizeof(send_buf), "Hello_%d_from_%d", msg_id++, getpid());
            printf("[Client] send: %s\n", send_buf);

            if (ipc_send(&ctx, send_buf, strlen(send_buf)) < 0) {
                perror("client send");
                break;
            }

            char recv_buf[4096];
            memset(recv_buf, 0, sizeof(recv_buf));
            int n = ipc_recv(&ctx, recv_buf, sizeof(recv_buf));
            if (n < 0) {
                perror("client recv");
                break;
            }
            printf("[Client] echo: %s\n", recv_buf);

            if (msg_id >= 10) running = 0;
        }
    }

    ipc_cleanup(&ctx);
    printf("[%s] %s exited\n", ipc_type_name(type),
           role == IPC_ROLE_SERVER ? "Server" : "Client");
    return 0;
}
EOF

# ==================== test/bench.c ====================
cat > ipc_lab/test/bench.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/wait.h>
#include "ipc.h"

#define ROUNDS 1000

static double now_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

static void bench_server(ipc_type_t type)
{
    ipc_context_t ctx;
    if (ipc_init(&ctx, type, IPC_ROLE_SERVER, NULL) < 0) {
        fprintf(stderr, "bench server init failed for %s\n", ipc_type_name(type));
        exit(1);
    }

    const size_t sizes[] = { 1, 64, 1024, 64 * 1024, 1024 * 1024 };
    const char   *labels[] = { "1B", "64B", "1KB", "64KB", "1MB" };
    const int     n_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < n_sizes; s++) {
        size_t pkt_len = sizes[s];
        char *buf = malloc(pkt_len);
        if (!buf) {
            perror("malloc");
            continue;
        }
        memset(buf, 0, pkt_len);

        for (int i = 0; i < ROUNDS; i++) {
            int n = ipc_recv(&ctx, buf, pkt_len);
            if (n < 0) {
                fprintf(stderr, "[%s Server] recv failed at round %d\n",
                        ipc_type_name(type), i);
                free(buf);
                ipc_cleanup(&ctx);
                exit(1);
            }
            if (ipc_send(&ctx, buf, pkt_len) < 0) {
                fprintf(stderr, "[%s Server] send failed at round %d\n",
                        ipc_type_name(type), i);
                free(buf);
                ipc_cleanup(&ctx);
                exit(1);
            }
        }
        free(buf);
        printf("[%s Server] completed %s x %d rounds\n",
               ipc_type_name(type), labels[s], ROUNDS);
    }

    ipc_cleanup(&ctx);
}

static void bench_client(ipc_type_t type)
{
    ipc_context_t ctx;
    if (ipc_init(&ctx, type, IPC_ROLE_CLIENT, NULL) < 0) {
        fprintf(stderr, "bench client init failed for %s\n", ipc_type_name(type));
        exit(1);
    }

    const size_t sizes[] = { 1, 64, 1024, 64 * 1024, 1024 * 1024 };
    const char   *labels[] = { "1B", "64B", "1KB", "64KB", "1MB" };
    const int     n_sizes = sizeof(sizes) / sizeof(sizes[0]);

    printf("\n=== %s Performance Results ===\n", ipc_type_name(type));
    printf("%-8s %-16s %-18s\n", "Size", "Avg Latency(us)", "Throughput(MB/s)");

    for (int s = 0; s < n_sizes; s++) {
        size_t pkt_len = sizes[s];
        char *send_buf = malloc(pkt_len);
        char *recv_buf = malloc(pkt_len);
        if (!send_buf || !recv_buf) {
            perror("malloc");
            free(send_buf);
            free(recv_buf);
            continue;
        }
        memset(send_buf, 'A', pkt_len);

        double latencies[ROUNDS];
        double t0 = now_ms();

        for (int i = 0; i < ROUNDS; i++) {
            double t_start = now_ms();

            if (ipc_send(&ctx, send_buf, pkt_len) < 0) {
                fprintf(stderr, "[%s Client] send failed at round %d\n",
                        ipc_type_name(type), i);
                free(send_buf);
                free(recv_buf);
                ipc_cleanup(&ctx);
                exit(1);
            }

            if (ipc_recv(&ctx, recv_buf, pkt_len) < 0) {
                fprintf(stderr, "[%s Client] recv failed at round %d\n",
                        ipc_type_name(type), i);
                free(send_buf);
                free(recv_buf);
                ipc_cleanup(&ctx);
                exit(1);
            }

            double t_end = now_ms();
            latencies[i] = (t_end - t_start) * 1000.0;
        }

        double t1 = now_ms();
        double total_time_s = (t1 - t0) / 1000.0;

        double sum = 0.0;
        for (int i = 0; i < ROUNDS; i++)
            sum += latencies[i];
        double avg_us = sum / ROUNDS;

        double total_bytes = (double)pkt_len * ROUNDS * 2;
        double throughput_mbps = total_bytes / (1024.0 * 1024.0) / total_time_s;

        printf("%-8s %-16.1f %-18.2f\n", labels[s], avg_us, throughput_mbps);
        fflush(stdout);

        free(send_buf);
        free(recv_buf);
    }

    ipc_cleanup(&ctx);
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ipc_type>\n", argv[0]);
        fprintf(stderr, "  ipc_type: fifo | msgqueue | shm | uds | all\n");
        return 1;
    }

    ipc_type_t types_to_test[IPC_TYPE_COUNT];
    int num_types = 0;

    if (strcmp(argv[1], "all") == 0) {
        for (int t = 0; t < IPC_TYPE_COUNT; t++)
            types_to_test[num_types++] = (ipc_type_t)t;
    } else {
        if (strcmp(argv[1], "fifo") == 0)
            types_to_test[num_types++] = IPC_TYPE_FIFO;
        else if (strcmp(argv[1], "msgqueue") == 0)
            types_to_test[num_types++] = IPC_TYPE_MSGQUEUE;
        else if (strcmp(argv[1], "shm") == 0)
            types_to_test[num_types++] = IPC_TYPE_SHM;
        else if (strcmp(argv[1], "uds") == 0)
            types_to_test[num_types++] = IPC_TYPE_UDS;
        else {
            fprintf(stderr, "Unknown IPC type: %s\n", argv[1]);
            return 1;
        }
    }

    for (int t = 0; t < num_types; t++) {
        ipc_type_t type = types_to_test[t];

        printf("\n================ Testing %s ================\n",
               ipc_type_name(type));

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }

        if (pid == 0) {
            sleep(1);
            bench_client(type);
            _exit(0);
        } else {
            bench_server(type);
            int status;
            waitpid(pid, &status, 0);
        }
    }

    return 0;
}
EOF

# ==================== Makefile ====================
cat > ipc_lab/Makefile << 'EOF'
UNAME_S  := $(shell uname -s)
CC       = gcc
CFLAGS   = -Wall -Wextra -Iinc -g -O2
LDFLAGS  = -lpthread -lm
ifeq ($(UNAME_S),Linux)
LDFLAGS  += -lrt
endif

SRCDIR   = src
TESTDIR  = test
OBJDIR   = obj
BINDIR   = bin

SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o, $(SRCS))

.PHONY: all clean

all: $(BINDIR) $(BINDIR)/ipc_test $(BINDIR)/bench

$(BINDIR):
	mkdir -p $(BINDIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR)/ipc_test: $(OBJS) $(TESTDIR)/ipc_test.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(TESTDIR)/ipc_test.c $(OBJS) $(LDFLAGS)

$(BINDIR)/bench: $(OBJS) $(TESTDIR)/bench.c | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(TESTDIR)/bench.c $(OBJS) $(LDFLAGS)

clean:
	rm -rf $(OBJDIR) $(BINDIR)
	rm -f /tmp/ipc_fifo_*
	rm -f /tmp/ipc_uds.sock
EOF

echo "=== 所有文件已创建 ==="
echo "编译中..."
cd ipc_lab && make
echo ""
echo "=== 编译完成! ==="
echo ""
echo "测试命令:"
echo "  终端1: ./bin/ipc_test fifo server"
echo "  终端2: ./bin/ipc_test fifo client"
echo "  性能测试: ./bin/bench fifo"
