#include <pthread.h>
#include <unistd.h>

#include "alarm_config.h"
#include "ipc_server.h"
#include "service_threads.h"

int main()
{
    alarm_config_init();
    ipc_server_init();

    pthread_t tid_ipc;
    pthread_t tid_mqtt;

    pthread_create(&tid_ipc, NULL, ipc_server_thread, NULL);
    pthread_create(&tid_mqtt, NULL, mqtt_server_thread, NULL);

    pthread_join(tid_ipc, NULL);
    pthread_join(tid_mqtt, NULL);

    return 0;
}
