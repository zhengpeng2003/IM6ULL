#ifndef SERVICE_THREADS_H
#define SERVICE_THREADS_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

void *ipc_server_thread(void *arg);
void *mqtt_server_thread(void *arg);
#ifdef __cplusplus
}
#endif

#endif // SERVICE_THREADS_H
