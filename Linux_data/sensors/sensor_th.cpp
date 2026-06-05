#include "sensor_th.hpp"
#include "ipc_server.h"
#include <stdio.h>

/*
 * ================================
 * RS485 业务级读寄存器封装
 * ================================
 * - 不再控制 RTS
 * - RTS 由 ModbusMaster + libmodbus 回调统一处理
 */
static int rs485_read_regs(
    ModbusMaster &bus,
    int slave,
    int addr,
    int nb,
    uint16_t *out)
{
    return bus.readRegisters(slave, addr, nb, out);
}

SensorTH::SensorTH(int slave_id, ModbusMaster &bus)
    : slave_id_(slave_id), bus_(bus)
{
}
void SensorTH::poll()
{
    uint16_t regs[2] = {0};
    char json[512];

    static int fail_count = 0;

    int ret = rs485_read_regs(bus_, slave_id_, 0x0000, 2, regs);
    if (ret != 2) {
        fail_count++;

        device_data_t dev = {};
        dev.device_id = slave_id_;
        dev.type      = DEV_SENSOR_TH;
        dev.valid     = (fail_count < 3);   // 连续 3 次失败才算无效

        // ⭐ 保留上一次有效数据
        dev.data.th.temperature = this->temp;
        dev.data.th.humidity    = this->humi;

        telemetry_pack_t pack = telemetry_pack_single(&dev);
        telemetry_pack_to_json(&pack, json, sizeof(json));
        ipc_server_send(json);

        return;
    }

    // 成功一次，失败计数清零
    fail_count = 0;

    this->temp = (int16_t)regs[1] / 10.0f;
    this->humi = regs[0] / 10.0f;

    device_data_t dev = {};
    dev.device_id = slave_id_;
    dev.type      = DEV_SENSOR_TH;
    dev.valid     = 1;
    dev.data.th.temperature = this->temp;
    dev.data.th.humidity    = this->humi;

    telemetry_pack_t pack = telemetry_pack_single(&dev);
    int len = telemetry_pack_to_json(&pack, json, sizeof(json));
    if (len > 0) {
    ipc_server_send(json);
    printf("[TH] id=%d temp=%.1f humi=%.1f\n",
               slave_id_, this->temp, this->humi);
    if(mqtt_send("imx6ull/device/data",json)==0)
	{
	printf("MQTT温湿度数据发送\n");
	}           
    }
}

float SensorTH::Gettemp()
{
    return this->temp;
}//温度
float SensorTH::Gethuimi()
{
    return this->humi;
}//温度
