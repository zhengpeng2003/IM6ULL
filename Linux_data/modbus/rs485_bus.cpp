#include "rs485_bus.hpp"

/* 真正的对象定义（只此一处） */
// RS485_1：不频繁，默认 RTS都要用
ModbusMaster RS485_1("/dev/ttymxc1", 38400, 22, RtsMode::CUSTOM);

// RS485_2：频繁读写，自定义 RTS
ModbusMaster RS485_2("/dev/ttymxc2", 9600, 23, RtsMode::CUSTOM);

