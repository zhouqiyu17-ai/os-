# Linux IPC 进程间通信实验

## 项目结构

```
.
├── inc/                  # 头文件
│   ├── ipc.h             # 通用 IPC 抽象接口
│   ├── fifo.h            # 命名管道接口
│   ├── msgqueue.h        # 消息队列接口 (System V)
│   ├── shm.h             # 共享内存接口
│   └── uds.h             # Unix 域套接字接口
├── src/                  # 源文件
│   ├── ipc.c             # IPC 抽象层实现
│   ├── fifo.c            # 命名管道实现
│   ├── msgqueue.c        # 消息队列实现 (System V msgget/msgsnd/msgrcv/msgctl)
│   ├── shm.c             # 共享内存实现 (System V shmget/shmat + semget)
│   └── uds.c             # Unix 域流式套接字实现
├── test/                 # 测试程序
│   ├── ipc_test.c        # 交互式测试程序
│   └── bench.c           # 性能基准测试程序
├── Makefile
└── README.md
```

## 编译 & 运行

### 环境要求

- **Linux** 操作系统 (System V 消息队列需要 Linux)
- macOS 下共享内存和命名管道/UDS 可正常编译运行，消息队列不支持 System V msgget，可改用 Docker/Linux VM

### 编译

```bash
make
```

编译产物在 `bin/` 目录下。

### 交互式测试

启动服务端：
```bash
./bin/ipc_test fifo server
./bin/ipc_test msgqueue server
./bin/ipc_test shm server
./bin/ipc_test uds server
```

启动客户端（另一个终端）：
```bash
./bin/ipc_test fifo client
./bin/ipc_test msgqueue client
./bin/ipc_test shm client
./bin/ipc_test uds client
```

### 性能测试

测试单一 IPC 类型：
```bash
./bin/bench fifo
./bin/bench msgqueue
./bin/bench shm
./bin/bench uds
```

测试所有 IPC 类型：
```bash
./bin/bench all
```

## IPC 实现说明

### 1. 命名管道 (FIFO)
- 服务端创建两个命名管道文件 `/tmp/ipc_fifo_tx` 和 `/tmp/ipc_fifo_rx`
- 服务端以写打开 tx、以读打开 rx；客户端以读打开 tx、以写打开 rx
- 数据帧格式：4 字节网络序长度 + 数据体

### 2. 消息队列
- 使用 System V 消息队列 (`msgget`/`msgsnd`/`msgrcv`/`msgctl`)
- 客户端发送类型为 1 的消息（正文包含自身 PID）
- 服务端接收类型为 1 的消息，将客户端 PID 作为回复消息类型
- 服务端捕获 SIGINT/SIGTERM 信号后删除消息队列

### 3. 共享内存
- 使用 System V 共享内存 (`shmget`/`shmat`/`shmctl`)
- 配合 System V 信号量 (`semget`/`semop`) 实现互斥访问
- 数据帧格式：4 字节长度头 + 数据体

### 4. Unix 域流式套接字
- 服务端创建 `AF_UNIX` + `SOCK_STREAM` 套接字
- 绑定到 `/tmp/ipc_uds.sock` 路径
- 基于 `read()`/`write()` 实现双向可靠通信

## 通用 IPC 接口

通过 `ipc_context_t` 结构体抽象不同 IPC 机制：

```c
ipc_context_t ctx;
ipc_init(&ctx, IPC_TYPE_FIFO, IPC_ROLE_SERVER, NULL);
ipc_send(&ctx, data, len);
ipc_recv(&ctx, buf, size);
ipc_cleanup(&ctx);
```

通过切换 `ipc_type_t` 枚举即可切换 IPC 通信方式。

## 性能测试

测试 1000 次往返通信，统计以下指标：
- 平均延迟 (微秒)
- 总吞吐量 (MB/s)

测试数据包大小：1B, 64B, 1KB, 64KB, 1MB

## 清洗

```bash
make clean
```
