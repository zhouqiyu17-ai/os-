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

    size_t msgsz = sizeof(long) + len;
    struct msgbuf_custom *msg = malloc(msgsz);
    if (!msg) return -1;
    memset(msg, 0, msgsz);
    msg->mtype = MSG_TYPE_BASE;
    memcpy(msg->mtext, buf, len);

    if (msgsnd(priv->msgqid, msg, len, 0) < 0) {
        perror("msgsnd");
        free(msg);
        return -1;
    }
    free(msg);
    return (int)len;
}

int msgqueue_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct msgqueue_priv *priv = ctx->priv;
    long recv_type = MSG_TYPE_BASE;

    if (len > MSG_MAX_LEN) return -1;

    size_t msgsz = sizeof(long) + MSG_MAX_LEN;
    struct msgbuf_custom *msg = malloc(msgsz);
    if (!msg) return -1;

    ssize_t n = msgrcv(priv->msgqid, msg, MSG_MAX_LEN, recv_type, 0);
    if (n < 0) {
        if (errno == EINTR) { free(msg); return -1; }
        perror("msgrcv");
        free(msg);
        return -1;
    }

    memcpy(buf, msg->mtext, (size_t)n);
    free(msg);
    return (int)n;
}
    return (int)len;
}

int msgqueue_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct msgqueue_priv *priv = ctx->priv;
    long recv_type = MSG_TYPE_BASE;

    if (len > MSG_MAX_LEN) return -1;

    struct msgbuf_custom msg;
    memset(&msg, 0, sizeof(msg));
    ssize_t n = msgrcv(priv->msgqid, &msg, MSG_MAX_LEN, recv_type, 0);
    if (n < 0) {
        if (errno == EINTR) return -1;
        perror("msgrcv");
        return -1;
    }



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
