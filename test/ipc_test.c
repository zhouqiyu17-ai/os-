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
