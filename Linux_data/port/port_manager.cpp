#include "port_manager.h"

#include "data_ack.h"
#include "data_publish.h"
#include "modbus_master.hpp"
#include "relay.hpp"
#include "sensor_th.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <dirent.h>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <thread>

namespace {

const int kSlotCount = 2;
const int kMinPollIntervalMs = 500;

enum class ManagedType {
    Unknown,
    SensorTh,
    Relay
};

struct ManagedDevice {
    int slave_id = 0;
    ManagedType type = ManagedType::Unknown;
    std::string type_name = "unknown";
    int poll_interval_ms = 0;
    std::chrono::steady_clock::time_point next_poll_time;
};

struct PortChannel {
    bool connected = false;
    std::string port;
    int baud = 0;
    std::unique_ptr<ModbusMaster> bus;
    std::vector<ManagedDevice> devices;
    bool poll_running = false;
    bool stop_requested = false;
    std::thread poll_thread;
    std::mutex io_lock;
};

bool startsWith(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

ManagedType parseType(const char *deviceType)
{
    if (!deviceType) return ManagedType::Unknown;
    if (strcmp(deviceType, "sensor_th") == 0) return ManagedType::SensorTh;
    if (strcmp(deviceType, "relay") == 0) return ManagedType::Relay;
    return ManagedType::Unknown;
}

const char *typeName(ManagedType type)
{
    switch (type) {
    case ManagedType::SensorTh: return "sensor_th";
    case ManagedType::Relay: return "relay";
    default: return "unknown";
    }
}

void setReason(char *reason, size_t reason_size, const char *value)
{
    if (!reason || reason_size == 0)
        return;

    snprintf(reason, reason_size, "%s", value ? value : "");
}

bool validSlot(int slot)
{
    return slot >= 0 && slot < kSlotCount;
}

std::vector<std::string> scanPorts()
{
    std::vector<std::string> ports;
    DIR *dir = opendir("/dev");
    if (!dir)
        return ports;

    while (dirent *entry = readdir(dir)) {
        const char *name = entry->d_name;
        if (startsWith(name, "ttyS") ||
            startsWith(name, "ttyUSB") ||
            startsWith(name, "ttymxc")) {
            ports.emplace_back(std::string("/dev/") + name);
        }
    }
    closedir(dir);

    std::sort(ports.begin(), ports.end());
    ports.erase(std::unique(ports.begin(), ports.end()), ports.end());
    return ports;
}

} // namespace

struct PortManager::Impl {
    std::array<PortChannel, kSlotCount> channels;
    std::mutex lock;

    bool isPortUsedByOtherSlot(int slot, const char *port)
    {
        std::lock_guard<std::mutex> guard(lock);
        for (int i = 0; i < kSlotCount; ++i) {
            if (i != slot && channels[i].connected && channels[i].port == port)
                return true;
        }
        return false;
    }

    std::unique_ptr<ModbusMaster> createBus(int slot, const char *port, int baud)
    {
        std::string port_name = port ? port : "";
        RtsMode mode = port_name.find("ttymxc") != std::string::npos
            ? RtsMode::CUSTOM
            : RtsMode::DEFAULT;
        return std::unique_ptr<ModbusMaster>(new ModbusMaster(port_name, baud, 22 + slot, mode));
    }

    void startPollingLocked(PortManager *owner, int slot)
    {
        PortChannel &channel = channels[slot];
        if (channel.poll_running)
            return;

        channel.stop_requested = false;
        channel.poll_running = true;
        channel.poll_thread = std::thread(&PortManager::Impl::pollThreadMain, this, owner, slot);
    }

    void stopPolling(int slot)
    {
        std::thread thread_to_join;

        {
            std::lock_guard<std::mutex> guard(lock);
            if (!validSlot(slot))
                return;

            PortChannel &channel = channels[slot];
            channel.stop_requested = true;
            if (channel.poll_thread.joinable())
                thread_to_join = std::move(channel.poll_thread);
        }

        if (thread_to_join.joinable())
            thread_to_join.join();

        std::lock_guard<std::mutex> guard(lock);
        if (validSlot(slot)) {
            channels[slot].poll_running = false;
            channels[slot].stop_requested = false;
        }
    }

    void clearSlot(int slot)
    {
        std::lock_guard<std::mutex> guard(lock);
        if (!validSlot(slot))
            return;

        PortChannel &channel = channels[slot];
        std::lock_guard<std::mutex> io_guard(channel.io_lock);
        channel.bus.reset();
        channel.devices.clear();
        channel.port.clear();
        channel.baud = 0;
        channel.connected = false;
        channel.stop_requested = false;
        channel.poll_running = false;
    }

    void pollThreadMain(PortManager *owner, int slot)
    {
        while (true) {
            {
                std::lock_guard<std::mutex> guard(lock);
                if (!validSlot(slot))
                    break;

                PortChannel &channel = channels[slot];
                if (channel.stop_requested || !channel.connected || !channel.bus)
                    break;
            }

            owner->pollSlot(slot);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        std::lock_guard<std::mutex> guard(lock);
        if (validSlot(slot)) {
            channels[slot].poll_running = false;
            channels[slot].stop_requested = false;
        }
    }

    void pollDevice(int slot, const ManagedDevice &device)
    {
        PortChannel *channel = nullptr;
        std::unique_lock<std::mutex> io_guard;

        {
            std::lock_guard<std::mutex> guard(lock);
            channel = &channels[slot];
            if (!channel->connected || !channel->bus)
                return;
            io_guard = std::unique_lock<std::mutex>(channel->io_lock);
        }

        device_data_t dev = {};
        int ret = -1;

        if (device.type == ManagedType::SensorTh)
            ret = sensor_th_read(*channel->bus, device.slave_id, &dev);
        else if (device.type == ManagedType::Relay)
            ret = relay_read_state(*channel->bus, device.slave_id, &dev);

        if (ret == 0 || dev.type != DEV_UNKNOWN)
            data_publish_device_status(&dev);
    }
};

PortManager::PortManager()
    : impl_(new Impl)
{}

PortManager::~PortManager()
{
    for (int slot = 0; slot < kSlotCount; ++slot)
        impl_->stopPolling(slot);
}

std::vector<std::string> PortManager::scanAvailablePorts()
{
    return scanPorts();
}

int PortManager::connectPort(int slot,
                             const char *port,
                             int baud,
                             char *reason,
                             size_t reason_size)
{
    if (!validSlot(slot) || !port || port[0] == '\0' || baud <= 0) {
        setReason(reason, reason_size, "invalid_request");
        return -1;
    }

    if (impl_->isPortUsedByOtherSlot(slot, port)) {
        setReason(reason, reason_size, "port_already_connected");
        return -1;
    }

    impl_->stopPolling(slot);
    impl_->clearSlot(slot);

    std::unique_ptr<ModbusMaster> bus = impl_->createBus(slot, port, baud);
    if (!bus || !bus->init()) {
        setReason(reason, reason_size, "open_failed");
        return -1;
    }

    {
        std::lock_guard<std::mutex> guard(impl_->lock);
        PortChannel &channel = impl_->channels[slot];
        channel.port = port;
        channel.baud = baud;
        channel.bus = std::move(bus);
        channel.connected = true;
        channel.stop_requested = false;
        impl_->startPollingLocked(this, slot);
    }

    setReason(reason, reason_size, "");
    return 0;
}

int PortManager::disconnectPort(int slot, char *reason, size_t reason_size)
{
    if (!validSlot(slot)) {
        setReason(reason, reason_size, "invalid_request");
        return -1;
    }

    {
        std::lock_guard<std::mutex> guard(impl_->lock);
        if (!impl_->channels[slot].connected) {
            setReason(reason, reason_size, "not_connected");
            return -1;
        }
    }

    impl_->stopPolling(slot);
    impl_->clearSlot(slot);

    setReason(reason, reason_size, "");
    return 0;
}

void PortManager::pollSlot(int slot)
{
    if (!validSlot(slot))
        return;

    std::vector<ManagedDevice> due_devices;
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> guard(impl_->lock);
        PortChannel &channel = impl_->channels[slot];
        if (!channel.connected || !channel.bus)
            return;

        for (ManagedDevice &device : channel.devices) {
            if (now < device.next_poll_time)
                continue;

            due_devices.push_back(device);
            device.next_poll_time = now + std::chrono::milliseconds(device.poll_interval_ms);
        }
    }

    for (const ManagedDevice &device : due_devices)
        impl_->pollDevice(slot, device);
}

int PortManager::addDevice(int slot,
                           int slave_id,
                           const char *device_type,
                           int poll_interval_ms,
                           char *reason,
                           size_t reason_size)
{
    if (!validSlot(slot) || slave_id <= 0) {
        setReason(reason, reason_size, "invalid_request");
        return -1;
    }

    ManagedType type = parseType(device_type);
    if (type == ManagedType::Unknown) {
        setReason(reason, reason_size, "unsupported_device_type");
        return -1;
    }

    if (poll_interval_ms < kMinPollIntervalMs) {
        setReason(reason, reason_size, "invalid_poll_interval");
        return -1;
    }

    std::lock_guard<std::mutex> guard(impl_->lock);
    PortChannel &channel = impl_->channels[slot];
    if (!channel.connected || !channel.bus) {
        setReason(reason, reason_size, "port_not_connected");
        return -1;
    }

    auto exists = std::find_if(channel.devices.begin(), channel.devices.end(),
                               [slave_id](const ManagedDevice &device) {
                                   return device.slave_id == slave_id;
                               });
    if (exists != channel.devices.end()) {
        setReason(reason, reason_size, "device_exists");
        return -1;
    }

    ManagedDevice device;
    device.slave_id = slave_id;
    device.type = type;
    device.type_name = typeName(type);
    device.poll_interval_ms = poll_interval_ms;
    device.next_poll_time = std::chrono::steady_clock::now();
    channel.devices.push_back(device);

    setReason(reason, reason_size, "");
    return 0;
}

int PortManager::removeDevice(int slot,
                              int slave_id,
                              char *reason,
                              size_t reason_size)
{
    if (!validSlot(slot) || slave_id <= 0) {
        setReason(reason, reason_size, "invalid_request");
        return -1;
    }

    std::lock_guard<std::mutex> guard(impl_->lock);
    PortChannel &channel = impl_->channels[slot];
    if (!channel.connected) {
        setReason(reason, reason_size, "port_not_connected");
        return -1;
    }

    auto it = std::find_if(channel.devices.begin(), channel.devices.end(),
                           [slave_id](const ManagedDevice &device) {
                               return device.slave_id == slave_id;
                           });
    if (it == channel.devices.end()) {
        setReason(reason, reason_size, "device_not_found");
        return -1;
    }

    channel.devices.erase(it);
    setReason(reason, reason_size, "");
    return 0;
}

int PortManager::handleRelay(int slot,
                             int slave_id,
                             const device_data_t *dev,
                             char *reason,
                             size_t reason_size)
{
    if (!validSlot(slot) || slave_id <= 0 || !dev || !dev->valid || dev->type != DEV_RELAY) {
        setReason(reason, reason_size, "invalid_request");
        return -1;
    }

    PortChannel *channel = nullptr;
    std::unique_lock<std::mutex> io_guard;

    {
        std::lock_guard<std::mutex> guard(impl_->lock);
        channel = &impl_->channels[slot];
        if (!channel->connected || !channel->bus) {
            setReason(reason, reason_size, "port_not_connected");
            return -1;
        }

        auto it = std::find_if(channel->devices.begin(), channel->devices.end(),
                               [slave_id](const ManagedDevice &device) {
                                   return device.slave_id == slave_id &&
                                          device.type == ManagedType::Relay;
                               });
        if (it == channel->devices.end()) {
            setReason(reason, reason_size, "relay_not_connected");
            return -1;
        }

        io_guard = std::unique_lock<std::mutex>(channel->io_lock);
    }

    if (relay_write_states(*channel->bus, slave_id, dev->data.relay.relay_states) != 0) {
        perror("[PortManager] write relay error");
        setReason(reason, reason_size, "modbus_write_failed");
        return -1;
    }

    setReason(reason, reason_size, "");
    return 0;
}

namespace {

PortManager g_manager;

} // namespace

extern "C" void port_manager_scan_ports(uint32_t seq, const char *cmd)
{
    std::vector<std::string> ports = PortManager::scanAvailablePorts();
    std::vector<const char *> port_names;
    port_names.reserve(ports.size());
    for (const std::string &port : ports)
        port_names.push_back(port.c_str());

    data_ack_send_ports(seq, cmd, port_names.data(), port_names.size());
}

extern "C" int port_manager_connect(int slot,
                                     const char *port,
                                     int baud,
                                     char *reason,
                                     size_t reason_size)
{
    return g_manager.connectPort(slot, port, baud, reason, reason_size);
}

extern "C" int port_manager_disconnect(int slot, char *reason, size_t reason_size)
{
    return g_manager.disconnectPort(slot, reason, reason_size);
}

extern "C" void port_manager_poll_slot(int slot)
{
    g_manager.pollSlot(slot);
}

extern "C" int port_manager_add_device(int slot,
                                        int slave_id,
                                        const char *device_type,
                                        int poll_interval_ms,
                                        char *reason,
                                        size_t reason_size)
{
    return g_manager.addDevice(slot, slave_id, device_type, poll_interval_ms, reason, reason_size);
}

extern "C" int port_manager_remove_device(int slot,
                                           int slave_id,
                                           char *reason,
                                           size_t reason_size)
{
    return g_manager.removeDevice(slot, slave_id, reason, reason_size);
}

extern "C" int port_manager_handle_relay(int slot,
                                          int slave_id,
                                          const device_data_t *dev,
                                          char *reason,
                                          size_t reason_size)
{
    return g_manager.handleRelay(slot, slave_id, dev, reason, reason_size);
}
