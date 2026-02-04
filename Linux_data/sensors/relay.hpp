// relay_controller.hpp
#pragma once
#include "modbus_master.hpp"
#include <cstdint>
#include <unistd.h>
#include <cstdio>
/* 继电器通道定义（bit 位） */
#define RELAY_LED_BIT     0   // bit0 -> LED
#define RELAY_FAN_BIT     1   // bit1 -> FAN
#define RELAY_BUZZER_BIT  2   // bit2 -> BUZZER

/* 位操作宏 */
#define RELAY_BIT_ON(states, bit)   ((states) |  (1 << (bit)))
#define RELAY_BIT_OFF(states, bit)  ((states) & ~(1 << (bit)))
#define RELAY_BIT_GET(states, bit)  (((states) >> (bit)) & 1)
class RelayController {
public:
    RelayController(int slave_id, ModbusMaster &bus);

    // 单独控制某一路继电器
    void setRelay(int index, bool on);

    // 控制全部继电器
    void allOn();
    void allOff();

    bool getRelay(int index);                 // 读单个
    void getAllRelays(uint8_t *states);       // 读全部
    // Poll 函数，循环测试继电器
    void poll(int delay_sec = 1);
private:
    int slave_id_;
    ModbusMaster &bus_;
    static const int RELAY_COUNT = 4;
};
