#ifndef DATA_COMMAND_H
#define DATA_COMMAND_H

#ifdef __cplusplus
extern "C" {
#endif

#define CMD_PROCESS_ERROR       -1
#define CMD_PROCESS_HANDLED      0
#define CMD_PROCESS_FORWARD_MQTT 1

int data_command_process_message(const char *json_str);

#ifdef __cplusplus
}
#endif

#endif
