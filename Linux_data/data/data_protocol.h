#ifndef DATA_PROTOCOL_H
#define DATA_PROTOCOL_H

#include <stdint.h>
#include <time.h>

/* 设备类型 */
typedef enum {
    DEV_SENSOR_TH = 1,   // 温湿度
    DEV_RELAY     = 2,   // 继电器
    DEV_SYSINFO  =100,   //设备额信息
} device_type_t;

/* 单个设备数据 */
typedef struct {
    int device_id;           // Modbus 从站地址
    device_type_t type;      // 设备类型
    time_t timestamp;        // 采集时间
    int valid;               // 1=有效 0=无效

    union {
        struct {
            float temperature;
            float humidity;
        } th;

        struct {
            uint16_t relay_states;   // 位图
        } relay;
	struct {
    char kernel[32];
    char arch[16];
    char os[32];
    int  screen_w;
    int  screen_h;
} sys;

    } data;

} device_data_t;

/* 一包数据（可多个设备） */
#define MAX_DEVICES_PER_PACK 8

typedef struct {
    uint32_t seq;
    time_t timestamp;
    int device_count;
    device_data_t devices[MAX_DEVICES_PER_PACK];
} data_pack_t;

#endif

