#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
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
    if (nr == 0) {
        errno = ECONNRESET;
        return -1;
    }
    if (nr != sizeof(net_len)) return -1;

    uint32_t pkt_len = ntohl(net_len);
    if (pkt_len > len) return -1;

    size_t total = 0;
    while (total < pkt_len) {
        ssize_t n = read(fd, (char *)buf + total, pkt_len - total);
        if (n == 0) {
            errno = ECONNRESET;
            return -1;
        }
        if (n < 0) return -1;
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
