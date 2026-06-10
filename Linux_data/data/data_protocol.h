#ifndef DATA_PROTOCOL_H
#define DATA_PROTOCOL_H

#include <stdint.h>
#include <time.h>

#define PROTOCOL_VER 1

#define MAX_DEVICES_PER_PACK 8
#define MAX_CLIENT_ID_LEN 32
#define MAX_CMD_NAME_LEN 32
#define MAX_ACK_MSG_LEN 64
#define MAX_DEVICE_NAME_LEN 64
#define MAX_THRESHOLD_POINTS_PER_DEVICE 8

typedef enum {
    MSG_TELEMETRY = 1,
    MSG_COMMAND   = 2,
    MSG_ACK       = 3
} msg_type_t;//消息类型

typedef enum {
    DEV_UNKNOWN   = 0,
    DEV_SENSOR_TH = 1,
    DEV_RELAY     = 2,
    DEV_ELECTRIC_METER = 3,
    DEV_SYSINFO   = 100
} device_type_t;//设备类型

typedef enum {
    ACK_OK = 0,
    ACK_ERROR = 1
} ack_status_t;//状态

typedef enum {
    DATA_SEND_OK = 0,
    DATA_SEND_INVALID_ARG = -1,
    DATA_SEND_JSON_ERROR = -2,
    DATA_SEND_IPC_NO_CLIENT = -3,
    DATA_SEND_IPC_WRITE_FAILED = -4,
    DATA_SEND_MQTT_NOT_READY = -5,
    DATA_SEND_MQTT_QUEUE_FULL = -6,
    DATA_SEND_PARTIAL_FAILED = -7
} data_send_code_t;//发送错误码

typedef struct {
    int device_id;
    char device_name[MAX_DEVICE_NAME_LEN];
    device_type_t type;
    int valid;
    char error_message[MAX_ACK_MSG_LEN];

    union {
        struct {
            float temperature;
            float humidity;
        } th;

        struct {
            uint16_t relay_states;
            int channel_count;
        } relay;

        struct {
            float voltage;
            float current;
            float power;
            float energy;
        } meter;

        struct {
            char kernel[32];
            char arch[16];
            char os[32];
            int screen_w;
            int screen_h;
            double cpu_usage;
            double memory_usage;
        } sys;
    } data;
} device_data_t;//设备数据

typedef struct {
    int enable_alarm;
    int has_low;
    float alarm_low;
    int has_high;
    float alarm_high;
} point_threshold_config_t;//单测点阈值配置

typedef struct {
    int threshold_enabled;
    point_threshold_config_t temperature;
    point_threshold_config_t humidity;
} sensor_threshold_config_t;//传感器阈值配置

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
