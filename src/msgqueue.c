#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/msg.h>
#include <signal.h>
#include <errno.h>
#include "msgqueue.h"

struct msgqueue_priv {
    int  msgqid;
    char *buf;
};

int msgqueue_server_init(ipc_context_t *ctx)
{
    struct msgqueue_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc msgqueue_priv");
        return -1;
    }

    priv->buf = malloc(MSG_MAX_LEN);
    if (!priv->buf) {
        perror("malloc msgqueue buf");
        free(priv);
        return -1;
    }

    int old = msgget(MSGQUEUE_KEY, 0666);
    if (old >= 0) msgctl(old, IPC_RMID, NULL);

    priv->msgqid = msgget(MSGQUEUE_KEY, IPC_CREAT | 0666);
    if (priv->msgqid < 0) {
        perror("msgget server");
        free(priv->buf);
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

    priv->buf = malloc(MSG_MAX_LEN);
    if (!priv->buf) {
        perror("malloc msgqueue buf");
        free(priv);
        return -1;
    }

    priv->msgqid = msgget(MSGQUEUE_KEY, 0666);
    if (priv->msgqid < 0) {
        perror("msgget client");
        free(priv->buf);
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

    long mtype = MSG_TYPE_BASE;
    memcpy(priv->buf + sizeof(long), buf, len);
    memcpy(priv->buf, &mtype, sizeof(long));

    if (msgsnd(priv->msgqid, priv->buf, len, 0) < 0) {
        perror("msgsnd");
        return -1;
    }
    return (int)len;
}

int msgqueue_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct msgqueue_priv *priv = ctx->priv;
    if (len > MSG_MAX_LEN) return -1;

    ssize_t n = msgrcv(priv->msgqid, priv->buf, MSG_MAX_LEN, MSG_TYPE_BASE, 0);
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
