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
        return;
    }

    const size_t sizes[] = { 1, 64, 1024, 64 * 1024, 1024 * 1024 };
    const char   *labels[] = { "1B", "64B", "1KB", "64KB", "1MB" };
    const int     n_sizes = sizeof(sizes) / sizeof(sizes[0]);

    for (int s = 0; s < n_sizes; s++) {
        size_t pkt_len = sizes[s];
        char *buf = malloc(pkt_len);
        if (!buf) { perror("malloc"); continue; }
        memset(buf, 0, pkt_len);

        int ok = 1;
        for (int i = 0; i < ROUNDS; i++) {
            int n = ipc_recv(&ctx, buf, pkt_len);
            if (n < 0) {
                fprintf(stderr, "[%s Server] recv failed at round %d\n",
                        ipc_type_name(type), i);
                ok = 0;
                break;
            }
            if (ipc_send(&ctx, buf, pkt_len) < 0) {
                fprintf(stderr, "[%s Server] send failed at round %d\n",
                        ipc_type_name(type), i);
                ok = 0;
                break;
            }
        }
        free(buf);
        if (!ok) break;
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
        return;
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
        int ok = 1;

        for (int i = 0; i < ROUNDS; i++) {
            double t_start = now_ms();

            if (ipc_send(&ctx, send_buf, pkt_len) < 0) {
                fprintf(stderr, "[%s Client] send failed at round %d\n",
                        ipc_type_name(type), i);
                ok = 0;
                break;
            }

            if (ipc_recv(&ctx, recv_buf, pkt_len) < 0) {
                fprintf(stderr, "[%s Client] recv failed at round %d\n",
                        ipc_type_name(type), i);
                ok = 0;
                break;
            }

            double t_end = now_ms();
            latencies[i] = (t_end - t_start) * 1000.0;
        }

        free(send_buf);
        free(recv_buf);
        if (!ok) break;

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
