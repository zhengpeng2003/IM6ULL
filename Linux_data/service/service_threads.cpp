#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "service_threads.h"
#include "rs485_bus.hpp"
#include "mqtt_wrapper.h"
/* ⭐⭐ 关键：声明 C 接口 ⭐⭐ */
extern "C" {
#include "ipc_server.h"
}

/*
 * RS485_1 线程
 */
void *rs485_1_thread(void *arg)
{
    (void)arg;
    while (1) {
        RS485_1.pollSlaves();
        sleep(3);
    }
    return NULL;
}

/*
 * RS485_2 线程
 */
void *rs485_2_thread(void *arg)
{
    (void)arg;
    while (1) {
        RS485_2.pollSlaves();
        sleep(2);
    } 
    return NULL;
}

/*
 * IPC Server 线程
 */
void *ipc_server_thread(void *arg)
{
    (void)arg;
    printf("开始循环接收前端数据\n");
    ipc_server_loop();   // ✅ 现在编译器认识它了
    return NULL;
}
/*
 * mqtt Server 线程
 */
void *mqtt_server_thread(void *arg)
{
    (void)arg;
    mqtt_init();
    printf("正在连接MQTT服务\n");
    while (1) {
    mqtt_poll();      // 10~50ms 调一次
    usleep(20000);
    }

}
