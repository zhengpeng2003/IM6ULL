#include "sensor_th.hpp"
#include "rs485_bus.hpp"
#include "relay.hpp"

// 全局对象，生命周期 = 程序整个运行期
SensorTH sensor2(1, RS485_2);
RelayController sensor1(1, RS485_1);
void init_sensors()
{
    RS485_2.addSlave(1, [] (int){
        sensor2.poll();
    });

    RS485_1.addSlave(1, [] (int){
   
    sensor1.poll();
    sleep(100);
  });
}

