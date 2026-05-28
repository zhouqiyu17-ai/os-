#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>
#include "msgqueue.h"

#define MSG_TYPE_C2S 1
#define MSG_TYPE_S2C 2

struct msgqueue_priv {
    int  msgqid;
    char *buf;
    size_t msgmax;
};

static volatile int recv_timeout;

static void timeout_handler(int sig) { (void)sig; recv_timeout = 1; }

static size_t get_msgmax(void)
{
    FILE *f = fopen("/proc/sys/kernel/msgmax", "r");
    unsigned long v = 8192;
    if (f) { fscanf(f, "%lu", &v); fclose(f); }
    if (v < (unsigned long)MSG_MAX_LEN) {
        f = fopen("/proc/sys/kernel/msgmax", "w");
        if (f) { fprintf(f, "%u\n", MSG_MAX_LEN); fclose(f); v = MSG_MAX_LEN; }
    }
    if (v > (unsigned long)MSG_MAX_LEN) v = MSG_MAX_LEN;
    return (size_t)v;
}

int msgqueue_server_init(ipc_context_t *ctx)
{
    struct msgqueue_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) { perror("calloc"); return -1; }

    priv->msgmax = get_msgmax();

    priv->buf = malloc(MSG_MAX_LEN);
    if (!priv->buf) { perror("malloc buf"); free(priv); return -1; }

    int old = msgget(MSGQUEUE_KEY, 0666);
    if (old >= 0) msgctl(old, IPC_RMID, NULL);

    priv->msgqid = msgget(MSGQUEUE_KEY, IPC_CREAT | 0666);
    if (priv->msgqid < 0) { perror("msgget server"); free(priv->buf); free(priv); return -1; }

    signal(SIGALRM, timeout_handler);

    ctx->priv = priv;
    ctx->ops.send    = msgqueue_send;
    ctx->ops.recv    = msgqueue_recv;
    ctx->ops.cleanup = msgqueue_cleanup;
    return 0;
}

int msgqueue_client_init(ipc_context_t *ctx)
{
    struct msgqueue_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) { perror("calloc"); return -1; }

    priv->msgmax = get_msgmax();

    priv->buf = malloc(MSG_MAX_LEN);
    if (!priv->buf) { perror("malloc buf"); free(priv); return -1; }

    priv->msgqid = msgget(MSGQUEUE_KEY, 0666);
    if (priv->msgqid < 0) { perror("msgget client"); free(priv->buf); free(priv); return -1; }

    ctx->priv = priv;
    ctx->ops.send    = msgqueue_send;
    ctx->ops.recv    = msgqueue_recv;
    ctx->ops.cleanup = msgqueue_cleanup;
    return 0;
}

int msgqueue_send(ipc_context_t *ctx, const void *buf, size_t len)
{
    struct msgqueue_priv *priv = ctx->priv;
    if (len > priv->msgmax) return -1;

    long mtype = (ctx->role == IPC_ROLE_CLIENT) ? MSG_TYPE_C2S : MSG_TYPE_S2C;
    memcpy(priv->buf, &mtype, sizeof(long));
    memcpy(priv->buf + sizeof(long), buf, len);

    if (msgsnd(priv->msgqid, priv->buf, len, 0) < 0) {
        if (errno != EINTR) perror("msgsnd");
        return -1;
    }
    return (int)len;
}

int msgqueue_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct msgqueue_priv *priv = ctx->priv;
    if (len > priv->msgmax) return -1;

    long mtype = (ctx->role == IPC_ROLE_SERVER) ? MSG_TYPE_C2S : MSG_TYPE_S2C;
    recv_timeout = 0;

    ssize_t n;
    if (ctx->role == IPC_ROLE_SERVER) {
        alarm(3);
        n = msgrcv(priv->msgqid, priv->buf, priv->msgmax, mtype, 0);
        alarm(0);
        if (recv_timeout) return -1;
    } else {
        n = msgrcv(priv->msgqid, priv->buf, priv->msgmax, mtype, 0);
    }

    if (n < 0) {
        if (errno == EINTR) return -1;
        perror("msgrcv");
        return -1;
    }
    if ((size_t)n > len) return -1;

    memcpy(buf, priv->buf + sizeof(long), (size_t)n);
    return (int)n;
}

void msgqueue_cleanup(ipc_context_t *ctx)
{
    struct msgqueue_priv *priv = ctx->priv;
    if (!priv) return;
    if (ctx->role == IPC_ROLE_SERVER && priv->msgqid >= 0)
        msgctl(priv->msgqid, IPC_RMID, NULL);
    free(priv->buf);
    free(priv);
    ctx->priv = NULL;
}
