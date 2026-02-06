// relay_controller.cpp
#include "relay.hpp"
#include <iostream>
#include "data_packer.h"
#include "ipc_server.h"
#include "data_packer.h"
#include <cstring>
RelayController::RelayController(int slave_id, ModbusMaster &bus)
    : slave_id_(slave_id), bus_(bus)
{
    printf("[RelayController] Initialized for slave %d\n", slave_id_);
}

// 单独控制某一路继电器
void RelayController::setRelay(int index, bool on)
{
    if (index < 0 || index >= RELAY_COUNT) return;

    uint8_t value = on ? 1 : 0;
    if (bus_.writeCoils(slave_id_, index, 1, &value) == -1) {
        perror("[Relay] writeCoils error");
    }
}

// 全部继电器开启
void RelayController::allOn()
{
    uint8_t on[RELAY_COUNT] = {1, 1, 1, 1};
    if (bus_.writeCoils(slave_id_, 0, RELAY_COUNT, on) == -1) {
        perror("[Relay] allOn error");
    } else {
        printf("[Relay] All ON\n");
    }
}

// 全部继电器关闭
void RelayController::allOff()
{
    uint8_t off[RELAY_COUNT] = {0, 0, 0, 0};
    if (bus_.writeCoils(slave_id_, 0, RELAY_COUNT, off) == -1) {
        perror("[Relay] allOff error");
    } else {
        printf("[Relay] All OFF\n");
    }
}
// Poll 函数，轮流测试继电器
void RelayController::poll(int delay_sec)

{
        device_data_t dev;
    memset(&dev, 0, sizeof(dev));

    dev.device_id = slave_id_;
    dev.type = DEV_RELAY;
    dev.valid = 0;

    uint8_t states[RELAY_COUNT] = {0};

    // 通过 ModbusMaster 读线圈
    if (bus_.readCoils(slave_id_, 0, RELAY_COUNT, states) == RELAY_COUNT) {
        dev.valid = 1;

        // 打包成位图
        uint16_t bitmap = 0;
        for (int i = 0; i < RELAY_COUNT; i++) {
            if (states[i])
                bitmap |= (1 << i);
        }

        dev.data.relay.relay_states = bitmap;
    }

    data_pack_t pack = data_pack_single(&dev);

    char json[256];
    int len = data_pack_to_json(&pack, json, sizeof(json));
    if (len > 0) {
        ipc_server_send(json);
}
}
//单个读
bool RelayController::getRelay(int index)
{
    if (index < 0 || index >= RELAY_COUNT)
        return false;

    uint8_t value = 0;
    if (bus_.readCoils(slave_id_, index, 1, &value) == -1) {
        perror("[Relay] readCoils error");
        return false;
    }

    return value == 1;
}
//读全部
void RelayController::getAllRelays(uint8_t *states)
{
    if (!states) return;

    if (bus_.readCoils(slave_id_, 0, RELAY_COUNT, states) == -1) {
        perror("[Relay] readAllCoils error");
    }
}
