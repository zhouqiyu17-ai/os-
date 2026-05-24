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

struct msgqueue_chunk {
    long   mtype;
    pid_t  sender_pid;
    size_t total_len;
    size_t offset;
    size_t chunk_len;
    char   data[];
};

int msgqueue_server_init(ipc_context_t *ctx)
{
    struct msgqueue_priv *priv = calloc(1, sizeof(*priv));
    if (!priv) {
        perror("calloc msgqueue_priv");
        return -1;
    }

    int old = msgget(MSGQUEUE_KEY, 0666);
    if (old >= 0) msgctl(old, IPC_RMID, NULL);

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
    size_t header_len = sizeof(struct msgqueue_chunk) - sizeof(long);
    size_t max_chunk;
    size_t offset = 0;
    long send_type;

    if (header_len >= MSG_MAX_LEN) {
        fprintf(stderr, "msgqueue_send: invalid chunk size\n");
        return -1;
    }
    max_chunk = MSG_MAX_LEN - header_len;

    if (ctx->role == IPC_ROLE_SERVER) {
        if (priv->peer_pid <= 0) {
            fprintf(stderr, "msgqueue_send: peer pid is unknown\n");
            return -1;
        }
        send_type = priv->peer_pid;
    } else {
        send_type = MSG_TYPE_BASE;
    }

    do {
        size_t chunk_len = len - offset;
        if (chunk_len > max_chunk)
            chunk_len = max_chunk;

        size_t alloc_len = sizeof(struct msgqueue_chunk) + chunk_len;
        struct msgqueue_chunk *msg = malloc(alloc_len);
        if (!msg) return -1;

        msg->mtype = send_type;
        msg->sender_pid = getpid();
        msg->total_len = len;
        msg->offset = offset;
        msg->chunk_len = chunk_len;
        if (chunk_len > 0)
            memcpy(msg->data, (const char *)buf + offset, chunk_len);

        if (msgsnd(priv->msgqid, msg, header_len + chunk_len, 0) < 0) {
            perror("msgsnd");
            free(msg);
            return -1;
        }

        free(msg);
        offset += chunk_len;
    } while (offset < len);

    return (int)len;
}

int msgqueue_recv(ipc_context_t *ctx, void *buf, size_t len)
{
    struct msgqueue_priv *priv = ctx->priv;
    long recv_type = (ctx->role == IPC_ROLE_SERVER) ? MSG_TYPE_BASE : getpid();
    size_t header_len = sizeof(struct msgqueue_chunk) - sizeof(long);
    size_t msgsz = sizeof(struct msgqueue_chunk) + MSG_MAX_LEN;
    size_t received = 0;
    size_t expected_len = 0;
    int have_expected_len = 0;
    int overflow = 0;

    struct msgqueue_chunk *msg = malloc(msgsz);
    if (!msg) return -1;

    do {
        ssize_t n = msgrcv(priv->msgqid, msg, MSG_MAX_LEN, recv_type, 0);
        if (n < 0) {
            if (errno != EINTR)
                perror("msgrcv");
            free(msg);
            return -1;
        }

        if ((size_t)n < header_len) {
            fprintf(stderr, "msgqueue_recv: malformed message\n");
            free(msg);
            return -1;
        }

        if (ctx->role == IPC_ROLE_SERVER)
            priv->peer_pid = msg->sender_pid;

        if (!have_expected_len) {
            expected_len = msg->total_len;
            have_expected_len = 1;
            if (expected_len > len)
                overflow = 1;
        }

        if (msg->total_len != expected_len ||
            msg->offset != received ||
            msg->chunk_len > (size_t)n - header_len ||
            msg->offset + msg->chunk_len > msg->total_len) {
            fprintf(stderr, "msgqueue_recv: malformed chunk\n");
            free(msg);
            return -1;
        }

        if (!overflow && msg->chunk_len > 0)
            memcpy((char *)buf + received, msg->data, msg->chunk_len);

        received += msg->chunk_len;
    } while (!have_expected_len || received < expected_len);

    free(msg);
    if (overflow)
        return -1;
    return (int)received;
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
