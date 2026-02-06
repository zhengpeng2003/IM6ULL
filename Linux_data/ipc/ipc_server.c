#include "ipc_server.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#define IPC_SOCK_PATH "/tmp/device_ipc.sock"
#define BUF_SIZE 1024
static int server_fd = -1;
static int client_fd = -1;
static pthread_mutex_t ipc_send_lock = PTHREAD_MUTEX_INITIALIZER;
int ipc_server_init(void)
{
    struct sockaddr_un addr;

    /* 1. 删除旧的 socket 文件 */
    unlink(IPC_SOCK_PATH);

    /* 2. 创建 socket */
    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }

    /* 3. 绑定地址 */
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, IPC_SOCK_PATH);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return -1;
    }

    /* 4. 监听 */
    if (listen(server_fd, 1) < 0) {
        perror("listen");
        return -1;
    }

    printf("IPC server listening on %s\n", IPC_SOCK_PATH);

    return 0;
}
void ipc_server_loop(void)
{
    char buf[BUF_SIZE];

    while (1) {
        if (client_fd < 0) {
            client_fd = accept(server_fd, NULL, NULL);
            if (client_fd >= 0) {
                fcntl(client_fd, F_SETFL, fcntl(client_fd, F_GETFL, 0) | O_NONBLOCK);
                printf("IPC client connected\n");
            }
        }

        if (client_fd >= 0) {
            int n = read(client_fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
		        printf("前端发送数据过来\n");
            if(mqtt_send("imx6ull/device/data",buf)==0)
	        {
		printf("继电器MQTT信息发送成功\n");
	    }
                // 只传给 Data 层，不在这里做类型判断
                data_process_message(buf);
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                close(client_fd);
                client_fd = -1;
            }
        }
        usleep(10000); // 10ms
    }
}
void ipc_server_send(const char *msg)
{
    if (client_fd < 0) return;
    pthread_mutex_lock(&ipc_send_lock);
    /* 中间 return / 出错 / goto 都要小心 */
    write(client_fd, msg, strlen(msg));
    write(client_fd, "\n", 1);
    pthread_mutex_unlock(&ipc_send_lock);
}

