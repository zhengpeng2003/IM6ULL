#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

int ipc_server_init(void);
void ipc_server_loop(void);
void ipc_server_send(const char *msg);
#ifdef __cplusplus
}
#endif

#endif

