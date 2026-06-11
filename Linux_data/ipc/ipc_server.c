#include "ipc_server.h"
#include "device_info.h"
#include "data_command.h"
#include "data_protocol.h"
#include "data_telemetry.h"
#include "mqtt_wrapper.h"
#include "OfflinePublishQueueC.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define IPC_SOCK_PATH "/tmp/device_ipc.sock"
#define BUF_SIZE 1024
#define RECV_BUF_SIZE 4096

static int server_fd = -1;
static int client_fd = -1;
static char recv_buf[RECV_BUF_SIZE];
static int recv_len = 0;
static pthread_mutex_t ipc_send_lock = PTHREAD_MUTEX_INITIALIZER;

int ipc_server_init(void)
{
    struct sockaddr_un addr;

    unlink(IPC_SOCK_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, IPC_SOCK_PATH);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }

    if (listen(server_fd, 1) < 0) {
        perror("listen");
        return -1;
    }

    printf("IPC server listening on %s\n", IPC_SOCK_PATH);
    return 0;
}

static void ipc_process_received_data(const char *buf, int len)
{
    if (recv_len + len >= RECV_BUF_SIZE)
        recv_len = 0;

    memcpy(recv_buf + recv_len, buf, len);
    recv_len += len;

    int start = 0;
    for (int i = 0; i < recv_len; ++i) {
        if (recv_buf[i] != '\n')
            continue;

        recv_buf[i] = '\0';
        if (i > start) {
            const char *msg = recv_buf + start;
            int ret = data_command_process_message(msg);
            if (ret == CMD_PROCESS_FORWARD_MQTT) {
                offline_publish_meta_t meta;
                memset(&meta, 0, sizeof(meta));
                meta.message_type = "ipc_forward";
                meta.gateway_id = DEFAULT_GATEWAY_ID;
                meta.priority = 0;
                (void)offline_publish_or_cache(MQTT_DEFAULT_PUBLISH_TOPIC, msg, &meta);
            } else if (ret == CMD_PROCESS_ERROR) {
                printf("IPC process failed, skip MQTT: %s\n", msg);
            }
        }
        start = i + 1;
    }

    if (start > 0) {
        memmove(recv_buf, recv_buf + start, recv_len - start);
        recv_len -= start;
    }
}

void ipc_server_loop(void)
{
    char buf[BUF_SIZE];

    while (1) {
        if (client_fd < 0) {
            client_fd = accept(server_fd, NULL, NULL);
            if (client_fd >= 0) {
                fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK);
                recv_len = 0;
                printf("IPC client connected\n");
                Deviceinfo_send();
            }
        }

        if (client_fd >= 0) {
            int n = read(client_fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                printf("IPC recv: %s\n", buf);
                ipc_process_received_data(buf, n);
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                close(client_fd);
                client_fd = -1;
                recv_len = 0;
            }
        }
        usleep(10000);
    }
}

int ipc_server_send(const char *msg)
{
    if (!msg)
        return DATA_SEND_INVALID_ARG;
    if (client_fd < 0)
        return DATA_SEND_IPC_NO_CLIENT;

    pthread_mutex_lock(&ipc_send_lock);
    ssize_t msg_written = write(client_fd, msg, strlen(msg));
    ssize_t nl_written = write(client_fd, "\n", 1);
    pthread_mutex_unlock(&ipc_send_lock);

    if (msg_written < 0 || nl_written < 0)
        return DATA_SEND_IPC_WRITE_FAILED;

    return DATA_SEND_OK;
}
