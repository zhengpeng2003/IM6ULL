#include <pthread.h>
#include <unistd.h>

#include "ipc_server.h"
#include "service_threads.h"

int main()
{
    ipc_server_init();

    pthread_t tid_rs485_1;
    pthread_t tid_rs485_2;
    pthread_t tid_ipc;
    pthread_t tid_mqtt;

    pthread_create(&tid_rs485_1, NULL, rs485_1_thread, NULL);
    pthread_create(&tid_rs485_2, NULL, rs485_2_thread, NULL);
    pthread_create(&tid_ipc, NULL, ipc_server_thread, NULL);
    pthread_create(&tid_mqtt, NULL, mqtt_server_thread, NULL);

    pthread_join(tid_rs485_1, NULL);
    pthread_join(tid_rs485_2, NULL);
    pthread_join(tid_ipc, NULL);
    pthread_join(tid_mqtt, NULL);

    return 0;
}
