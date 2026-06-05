#ifndef DATA_PROTOCOL_H
#define DATA_PROTOCOL_H

#include <stdint.h>
#include <time.h>

#define PROTOCOL_VER 1

#define MAX_DEVICES_PER_PACK 8
#define MAX_CLIENT_ID_LEN 32
#define MAX_CMD_NAME_LEN 32
#define MAX_ACK_MSG_LEN 64

typedef enum {
    MSG_TELEMETRY = 1,
    MSG_COMMAND   = 2,
    MSG_ACK       = 3
} msg_type_t;//消息类型

typedef enum {
    DEV_UNKNOWN   = 0,
    DEV_SENSOR_TH = 1,
    DEV_RELAY     = 2,
    DEV_SYSINFO   = 100
} device_type_t;//设备类型

typedef enum {
    ACK_OK = 0,
    ACK_ERROR = 1
} ack_status_t;//状态

typedef struct {
    int device_id;
    device_type_t type;
    int valid;

    union {
        struct {
            float temperature;
            float humidity;
        } th;

        struct {
            uint16_t relay_states;
        } relay;

        struct {
            char kernel[32];
            char arch[16];
            char os[32];
            int screen_w;
            int screen_h;
        } sys;
    } data;
} device_data_t;//设备数据

typedef struct {
    int device_count;
    device_data_t devices[MAX_DEVICES_PER_PACK];
} telemetry_msg_t;//消息发送包

typedef struct {
    char cmd[MAX_CMD_NAME_LEN];
    int device_id;
    int channel;
    int value;
} command_msg_t;//command请求包

typedef struct {
    char cmd[MAX_CMD_NAME_LEN];//发送的命令
    ack_status_t status;
    char message[MAX_ACK_MSG_LEN];
} ack_msg_t;//ack回复包

typedef struct {
    uint32_t ver;
    uint32_t seq;
    time_t time;
    msg_type_t type;

    char client_id[MAX_CLIENT_ID_LEN];
    char target_id[MAX_CLIENT_ID_LEN];

    union {
        telemetry_msg_t telemetry;
        command_msg_t command;
        ack_msg_t ack;
    } body;
} protocol_msg_t;//统一数据包

#endif
