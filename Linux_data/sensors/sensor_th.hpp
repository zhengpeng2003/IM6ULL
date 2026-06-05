#pragma once
#include "modbus_master.hpp"
#include "data_telemetry.h"
#include "mqtt_wrapper.h"

class SensorTH
{
public:
    SensorTH(int slave_id, ModbusMaster &bus);
    float Gettemp();
    float Gethuimi();
    void poll();

private:
    int slave_id_;
    float temp=0;//温度
    float humi=0;//湿度
    ModbusMaster &bus_;  // 指定总线
};
