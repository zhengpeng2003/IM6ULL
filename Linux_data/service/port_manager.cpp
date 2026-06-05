#include "port_manager.h"

#include "data_port.h"
#include "data_telemetry.h"
#include "ipc_server.h"
#include "modbus_master.hpp"

#include <algorithm>
#include <array>
#include <dirent.h>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

namespace {

enum class BoundType {
    Unknown,
    SensorTh,
    Relay
};

struct PortSlot {
    bool connected = false;
    std::string port;
    std::string typeName = "unknown";
    BoundType type = BoundType::Unknown;
    int baud = 0;
    std::unique_ptr<ModbusMaster> bus;
};

std::array<PortSlot, 2> g_slots;//PortSlot g_slots[2];等价
std::mutex g_lock;

bool startsWith(const char *text, const char *prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

BoundType parseType(const char *deviceType)
{
    if (!deviceType) return BoundType::Unknown;
    if (strcmp(deviceType, "sensor_th") == 0) return BoundType::SensorTh;
    if (strcmp(deviceType, "relay") == 0) return BoundType::Relay;
    return BoundType::Unknown;
}

const char *typeName(BoundType type)
{
    switch (type) {
    case BoundType::SensorTh: return "sensor_th";
    case BoundType::Relay: return "relay";
    default: return "unknown";
    }
}

void sendStatus(int slot,
                const char *port,
                const char *deviceType,
                int baud,
                bool connected,
                const char *message)
{
    char json[512];
    if (data_port_pack_status_json(slot,
                                   port,
                                   deviceType,
                                   baud,
                                   connected,
                                   message,
                                   json,
                                   sizeof(json)) > 0) {
        ipc_server_send(json);
    }
}

void sendDevice(const device_data_t &dev)
{
    char json[512];
    telemetry_pack_t pack = telemetry_pack_single(&dev);
    if (telemetry_pack_to_json(&pack, json, sizeof(json)) > 0)
        ipc_server_send(json);
}

std::vector<std::string> scanPortsLocked()
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

void pollSensor(PortSlot &slot)
{
    uint16_t regs[2] = {0};
    device_data_t dev = {};
    dev.device_id = 1;
    dev.type = DEV_SENSOR_TH;
    int ret = slot.bus->readRegisters(1, 0x0000, 2, regs);
    dev.valid = (ret == 2);
    if (dev.valid) {
        dev.data.th.temperature = static_cast<int16_t>(regs[1]) / 10.0f;
        dev.data.th.humidity = regs[0] / 10.0f;
    }
    sendDevice(dev);
}

void pollRelay(PortSlot &slot)
{
    uint8_t states[4] = {0};
    device_data_t dev = {};
    dev.device_id = 1;
    dev.type = DEV_RELAY;
    int ret = slot.bus->readCoils(1, 0, 4, states);
    dev.valid = (ret == 4);
    if (dev.valid) {
        uint16_t bitmap = 0;
        for (int i = 0; i < 4; ++i) {
            if (states[i])
                bitmap |= (1 << i);
        }
        dev.data.relay.relay_states = bitmap;
    }
    sendDevice(dev);
}

} // namespace

extern "C" void port_manager_scan_ports(void)
{
    std::lock_guard<std::mutex> lock(g_lock);
    std::vector<std::string> ports = scanPortsLocked();

    char json[1024];
    if (data_port_pack_ports_json(ports, json, sizeof(json)) > 0)
        ipc_server_send(json);
}

extern "C" void port_manager_connect(int slot, const char *port, const char *device_type, int baud)
{
    if (slot < 0 || slot >= static_cast<int>(g_slots.size()) || !port || baud <= 0) {
        sendStatus(slot, port, device_type, baud, false, "invalid_request");
        return;
    }

    std::lock_guard<std::mutex> lock(g_lock);
    for (int i = 0; i < static_cast<int>(g_slots.size()); ++i) {
        if (i != slot && g_slots[i].connected && g_slots[i].port == port) {
            sendStatus(slot, port, device_type, baud, false, "port_already_connected");
            return;
        }
    }

    PortSlot &target = g_slots[slot];
    target.bus.reset();
    target = PortSlot();
    target.port = port;
    target.type = parseType(device_type);
    target.typeName = typeName(target.type);
    target.baud = baud;

    RtsMode mode = target.port.find("ttymxc") != std::string::npos
        ? RtsMode::CUSTOM
        : RtsMode::DEFAULT;
    target.bus.reset(new ModbusMaster(target.port, target.baud, 22 + slot, mode));
    if (!target.bus->init()) {
        target.bus.reset();
        sendStatus(slot, port, target.typeName.c_str(), baud, false, "open_failed");
        return;
    }

    target.connected = true;
    sendStatus(slot, target.port.c_str(), target.typeName.c_str(), target.baud, true, "connected");
}

extern "C" void port_manager_disconnect(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(g_slots.size()))
        return;

    std::lock_guard<std::mutex> lock(g_lock);
    PortSlot &target = g_slots[slot];
    std::string oldPort = target.port;
    std::string oldType = target.typeName;
    int oldBaud = target.baud;
    target.bus.reset();
    target = PortSlot();
    sendStatus(slot, oldPort.c_str(), oldType.c_str(), oldBaud, false, "disconnected");
}

extern "C" void port_manager_poll_slot(int slot)
{
    std::lock_guard<std::mutex> lock(g_lock);
    if (slot < 0 || slot >= static_cast<int>(g_slots.size()))
        return;

    PortSlot &target = g_slots[slot];
    if (!target.connected || !target.bus)
        return;

    if (target.type == BoundType::SensorTh)
        pollSensor(target);
    else if (target.type == BoundType::Relay)
        pollRelay(target);
}

extern "C" void port_manager_handle_relay(const device_data_t *dev)
{
    if (!dev || !dev->valid)
        return;

    std::lock_guard<std::mutex> lock(g_lock);
    for (PortSlot &slot : g_slots) {
        if (!slot.connected || slot.type != BoundType::Relay || !slot.bus)
            continue;

        uint16_t states = dev->data.relay.relay_states;
        for (int i = 0; i < 16; ++i) {
            uint8_t v = (states & (1 << i)) ? 1 : 0;
            slot.bus->writeCoils(1, i, 1, &v);
        }
        return;
    }
}
