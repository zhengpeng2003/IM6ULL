#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

#include "service_threads.h"
#include "mqtt_wrapper.h"

extern "C" {
#include "ipc_server.h"
}

void *ipc_server_thread(void *arg)
{
    (void)arg;
    printf("start IPC receive loop\n");
    ipc_server_loop();
    return NULL;
}

void *mqtt_server_thread(void *arg)
{
    (void)arg;
    mqtt_init();
    printf("connecting MQTT service\n");

    while (1) {
        mqtt_poll();
        usleep(20000);
    }

    return NULL;
}
