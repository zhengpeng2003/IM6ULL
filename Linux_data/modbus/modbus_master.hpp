#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <modbus/modbus.h>

enum class RtsMode {
    DEFAULT,   // libmodbus 默认
    CUSTOM     // 自定义 GPIO + 回调
};

class ModbusMaster
{
public:
    ModbusMaster(const std::string &dev, int baud, int gpio, RtsMode mode = RtsMode::DEFAULT);
    ~ModbusMaster();

    bool init();
    void close();

    void setRts(int tx); // GPIO 控制 RTS，仅 CUSTOM 模式有效

    void addSlave(int slave_id, std::function<void(int)> cb);
    void pollSlaves();

    int readRegisters(int slave, int addr, int nb, uint16_t *out);
    int writeRegister(int slave, int addr, uint16_t value);
    int writeCoils(int slave, int addr, int nb, const uint8_t *data);
int readCoils(int slave, int addr, int nb, uint8_t *out);

private:
    bool gpioInit();

    static void rtsCallback(modbus_t *ctx, int on);
    static void registerCtx(modbus_t *ctx, ModbusMaster *self);
    static void unregisterCtx(modbus_t *ctx);
    static std::map<modbus_t *, ModbusMaster *> ctx_map_;

private:
    struct Slave {
        int id;
        std::function<void(int)> callback;
    };

    std::vector<Slave> slaves_;

    std::string dev_;
    int baud_;
    int gpio_;
    int gpio_fd_;
    modbus_t *ctx_;
    RtsMode rts_mode_;
};

