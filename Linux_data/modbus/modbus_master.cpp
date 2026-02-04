#include "modbus_master.hpp"

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>

/* ctx → 对象映射 */
std::map<modbus_t *, ModbusMaster *> ModbusMaster::ctx_map_;

ModbusMaster::ModbusMaster(const std::string &dev, int baud, int gpio, RtsMode mode)
    : dev_(dev),
      baud_(baud),
      gpio_(gpio),
      gpio_fd_(-1),
      ctx_(nullptr),
      rts_mode_(mode)
{}

ModbusMaster::~ModbusMaster()
{
    close();
}

bool ModbusMaster::gpioInit()
{
    if (rts_mode_ != RtsMode::CUSTOM)
        return true;  // 简单模式不初始化 GPIO

    char path[128];
    char num[8];
    snprintf(num, sizeof(num), "%d", gpio_);

    int fd = ::open("/sys/class/gpio/export", O_WRONLY);
    if (fd >= 0) { ::write(fd, num, strlen(num)); ::close(fd); usleep(100000); }

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", gpio_);
    fd = ::open(path, O_WRONLY);
    if (fd < 0) { perror("gpio direction"); return false; }
    ::write(fd, "out", 3); ::close(fd);

    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", gpio_);
    gpio_fd_ = ::open(path, O_WRONLY);
    if (gpio_fd_ < 0) { perror("gpio value"); return false; }

    // 默认 RX
    ::write(gpio_fd_, "1", 1);
    return true;
}

void ModbusMaster::setRts(int tx)
{
    if (rts_mode_ != RtsMode::CUSTOM || gpio_fd_ < 0) return;
    ::write(gpio_fd_, tx ? "0" : "1", 1);
}

void ModbusMaster::registerCtx(modbus_t *ctx, ModbusMaster *self)
{
    ctx_map_[ctx] = self;
}

void ModbusMaster::unregisterCtx(modbus_t *ctx)
{
    ctx_map_.erase(ctx);
}

void ModbusMaster::rtsCallback(modbus_t *ctx, int on)
{
    auto it = ctx_map_.find(ctx);
    if (it == ctx_map_.end()) return;
    ModbusMaster *self = it->second;

    self->setRts(on);
    if (!on) {
        tcdrain(modbus_get_socket(ctx));
        usleep(1500);
    }
}

bool ModbusMaster::init()
{
    if (!gpioInit()) return false;

    ctx_ = modbus_new_rtu(dev_.c_str(), baud_, 'N', 8, 1);
    if (!ctx_) { perror("modbus_new_rtu"); return false; }

    if (rts_mode_ == RtsMode::CUSTOM) {
        registerCtx(ctx_, this);
        modbus_rtu_set_custom_rts(ctx_, rtsCallback);
        modbus_rtu_set_rts(ctx_, MODBUS_RTU_RTS_DOWN);
        modbus_rtu_set_serial_mode(ctx_, MODBUS_RTU_RS485);
    }

    if (modbus_connect(ctx_) == -1) {
        perror("modbus_connect");
        if (rts_mode_ == RtsMode::CUSTOM) unregisterCtx(ctx_);
        modbus_free(ctx_);
        ctx_ = nullptr;
        return false;
    }

    printf("[Modbus] %s baud=%d gpio=%d mode=%s ready\n",
           dev_.c_str(), baud_, gpio_,
           rts_mode_ == RtsMode::CUSTOM ? "CUSTOM" : "DEFAULT");

    return true;
}

void ModbusMaster::close()
{
    if (ctx_) {
        if (rts_mode_ == RtsMode::CUSTOM) unregisterCtx(ctx_);
        modbus_close(ctx_);
        modbus_free(ctx_);
        ctx_ = nullptr;
    }
    if (gpio_fd_ >= 0) { ::close(gpio_fd_); gpio_fd_ = -1; }
}

void ModbusMaster::addSlave(int slave_id, std::function<void(int)> cb)
{
    slaves_.push_back({slave_id, cb});
}

void ModbusMaster::pollSlaves()
{
    for (auto &s : slaves_) if (s.callback) s.callback(s.id);
}

int ModbusMaster::readRegisters(int slave, int addr, int nb, uint16_t *out)
{
    if (!ctx_) return -1;
    modbus_set_slave(ctx_, slave);
    return modbus_read_registers(ctx_, addr, nb, out);
}
int ModbusMaster::readCoils(int slave, int addr, int nb, uint8_t *out)
{
    if (!ctx_) return -1;
    modbus_set_slave(ctx_, slave);
    return modbus_read_bits(ctx_, addr, nb, out);
}

int ModbusMaster::writeRegister(int slave, int addr, uint16_t value)
{
    if (!ctx_) return -1;
    modbus_set_slave(ctx_, slave);
    return modbus_write_register(ctx_, addr, value);
}

int ModbusMaster::writeCoils(int slave, int addr, int nb, const uint8_t *data)
{
    if (!ctx_) return -1;
    modbus_set_slave(ctx_, slave);
    return modbus_write_bits(ctx_, addr, nb, data);
}

