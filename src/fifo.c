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
