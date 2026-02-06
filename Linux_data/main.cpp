#include <unistd.h>
#include "rs485_bus.hpp"
#include "sensor_th.hpp"
#include "ipc_server.h"
#include "service_threads.h"
#include "mqtt/wrapper/mqtt_wrapper.h"
#include "sensors/init_sensors.hpp"
#include "device/device_info.hpp"
int main()
{
    /* === 系统服务初始化 === */
    ipc_server_init();
    /* === 设备初始化（注册到总线） === */
    init_sensors();
    /* === 总线初始化 === */
    if (!RS485_1.init()) {
        return -1;
    }
    if (!RS485_2.init()) {
        return -1;
    }
    /* === 设备初始化（注册到总线） === */
    init_sensors();
    pthread_t tid_rs485_1;
    pthread_t tid_rs485_2;
    pthread_t tid_ipc;
    pthread_t tid_mqtt;

    pthread_create(&tid_rs485_1, NULL, rs485_1_thread, NULL);
    pthread_create(&tid_rs485_2, NULL, rs485_2_thread, NULL);
    pthread_create(&tid_ipc, NULL, ipc_server_thread, NULL);
    pthread_create(&tid_mqtt, NULL, mqtt_server_thread, NULL);
    // 主线程一般不退出
    pthread_join(tid_rs485_1, NULL);
    pthread_join(tid_rs485_2, NULL);
    pthread_join(tid_ipc, NULL);
    pthread_join(tid_mqtt, NULL);

    /* === 理论上不会走到，但保持完整 === */
    RS485_1.close();
    RS485_2.close();

    return 0;
}

