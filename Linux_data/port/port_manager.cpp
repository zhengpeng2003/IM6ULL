#include "port_manager.h"

#include "data_ack.h"
#include "data_publish.h"
#include "data_telemetry.h"
#include "ipc_server.h"
#include "modbus_master.hpp"
#include "mqtt_wrapper.h"
#include "OfflinePublishQueueC.h"
#include "relay.hpp"
#include "sensor_device.hpp"
#include "temperature_humidity_sensor.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <dirent.h>
#include <json-c/json.h>
#include <memory>
#include <mutex>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>

namespace {

const int kSlotCount = 2;
const int kMinPollIntervalMs = 500;
const char kDeviceConfigPath[] = "/etc/qt_object/device_config.json";
const char kDeviceConfigDir[] = "/etc/qt_object";

enum class ManagedType {
    Unknown,
    SensorTh,
    Relay
};

struct ManagedDevice {
    std::shared_ptr<SensorDevice> sensor;
    std::chrono::steady_clock::time_point next_poll_time;
    device_data_t last_data = {};
    bool has_last_data = false;
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

int64_t currentTimeMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

void addString(json_object *obj, const char *key, const char *value)
{
    json_object_object_add(obj, key, json_object_new_string(value ? value : ""));
}
//这个是错误的
// void addNullableDouble(json_object *obj, const char *key, int has_value, float value)
// {
//     json_object_object_add(obj,
//                            key,
//                            has_value ? json_object_new_double(value) : json_object_new_null());
// }
void addNullableDouble(json_object *obj, const char *key, int has_value, float value)
{
    json_object *json_value = nullptr;

    if (has_value) {
        json_value = json_object_new_double(value);
    }

    json_object_object_add(obj, key, json_value);
}

json_object *thresholdPointToJson(const point_threshold_config_t &point)
{
    json_object *obj = json_object_new_object();
    if (!obj)
        return nullptr;

    json_object_object_add(obj, "enable_alarm", json_object_new_boolean(point.enable_alarm));
    json_object_object_add(obj, "enableAlarm", json_object_new_boolean(point.enable_alarm));
    addNullableDouble(obj, "alarm_low", point.has_low, point.alarm_low);
    addNullableDouble(obj, "alarmLow", point.has_low, point.alarm_low);
    addNullableDouble(obj, "alarm_high", point.has_high, point.alarm_high);
    addNullableDouble(obj, "alarmHigh", point.has_high, point.alarm_high);
    return obj;
}

void thresholdPointFromJson(json_object *obj, point_threshold_config_t *point)
{
    if (!obj || !point)
        return;

    json_object *v = nullptr;
    if (json_object_object_get_ex(obj, "enable_alarm", &v) ||
        json_object_object_get_ex(obj, "enableAlarm", &v)) {
        point->enable_alarm = json_object_get_boolean(v);
    }
    if (json_object_object_get_ex(obj, "alarm_low", &v) ||
        json_object_object_get_ex(obj, "alarmLow", &v)) {
        if (!json_object_is_type(v, json_type_null)) {
            point->has_low = 1;
            point->alarm_low = (float)json_object_get_double(v);
        }
    }
    if (json_object_object_get_ex(obj, "alarm_high", &v) ||
        json_object_object_get_ex(obj, "alarmHigh", &v)) {
        if (!json_object_is_type(v, json_type_null)) {
            point->has_high = 1;
            point->alarm_high = (float)json_object_get_double(v);
        }
    }
}

void thresholdConfigFromJson(json_object *root, sensor_threshold_config_t *config)
{
    if (!root || !config)
        return;

    memset(config, 0, sizeof(*config));
    json_object *v = nullptr;
    if (json_object_object_get_ex(root, "threshold_enabled", &v) ||
        json_object_object_get_ex(root, "thresholdEnabled", &v)) {
        config->threshold_enabled = json_object_get_boolean(v);
    }

    json_object *thresholds = nullptr;
    if (!json_object_object_get_ex(root, "thresholds", &thresholds))
        thresholds = root;

    json_object *point = nullptr;
    if (json_object_object_get_ex(thresholds, "temperature", &point))
        thresholdPointFromJson(point, &config->temperature);
    if (json_object_object_get_ex(thresholds, "humidity", &point))
        thresholdPointFromJson(point, &config->humidity);
}

json_object *thresholdConfigToJson(const sensor_threshold_config_t &config)
{
    json_object *obj = json_object_new_object();
    if (!obj)
        return nullptr;

    json_object_object_add(obj, "threshold_enabled", json_object_new_boolean(config.threshold_enabled));
    json_object_object_add(obj, "thresholdEnabled", json_object_new_boolean(config.threshold_enabled));

    json_object *thresholds = json_object_new_object();
    if (thresholds) {
        json_object *temperature = thresholdPointToJson(config.temperature);
        json_object *humidity = thresholdPointToJson(config.humidity);
        if (temperature)
            json_object_object_add(thresholds, "temperature", temperature);
        if (humidity)
            json_object_object_add(thresholds, "humidity", humidity);
        json_object_object_add(obj, "thresholds", thresholds);
    }
    return obj;
}

std::shared_ptr<SensorDevice> createSensorDevice(ManagedType type,
                                                 int slave_id,
                                                 int poll_interval_ms,
                                                 const sensor_threshold_config_t *threshold_config)
{
    if (type == ManagedType::SensorTh)
        return std::make_shared<TemperatureHumiditySensor>(slave_id, poll_interval_ms, threshold_config);
    if (type == ManagedType::Relay)
        return std::make_shared<RelayDevice>(slave_id, poll_interval_ms);
    return std::shared_ptr<SensorDevice>();
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

    void publishThresholdConfig(int slot, const ManagedDevice &device)
    {
        if (!device.sensor)
            return;

        sensor_threshold_config_t config = {};
        if (!device.sensor->thresholdConfig(&config))
            return;

        PortChannel &channel = channels[slot];

        json_object *root = json_object_new_object();
        if (!root)
            return;

        addString(root, "type", "threshold_config");
        json_object_object_add(root, "version", json_object_new_int(PROTOCOL_VER));
        json_object_object_add(root, "timestampMs", json_object_new_int64(currentTimeMs()));
        addString(root, "sourceId", DEFAULT_SOURCE_ID);
        addString(root, "targetId", DEFAULT_TARGET_ID);

        json_object *site = json_object_new_object();
        if (site) {
            addString(site, "factoryId", DEFAULT_FACTORY_ID);
            addString(site, "factoryName", DEFAULT_FACTORY_NAME);
            addString(site, "areaId", DEFAULT_AREA_ID);
            addString(site, "areaName", DEFAULT_AREA_NAME);
            addString(site, "gatewayId", DEFAULT_GATEWAY_ID);
            addString(site, "gatewayName", DEFAULT_GATEWAY_NAME);
            addString(site, "portId", slot == 0 ? "port_001" : "port_002");
            addString(site, "portName", slot == 0 ? "RS485-1" : "RS485-2");
            json_object_object_add(root, "site", site);
        }

        json_object *devices = json_object_new_array();
        json_object *dev = json_object_new_object();
        if (devices && dev) {
            json_object_object_add(dev, "slot", json_object_new_int(slot));
            addString(dev, "port", channel.port.c_str());
            json_object_object_add(dev, "baud", json_object_new_int(channel.baud));
            json_object_object_add(dev, "deviceId", json_object_new_int(device.sensor->slaveId()));
            addString(dev, "deviceName", "");
            addString(dev, "deviceType", device.sensor->deviceTypeName());
            json_object_object_add(dev, "thresholdEnabled", json_object_new_boolean(config.threshold_enabled));

            json_object *points = json_object_new_array();
            if (points) {
                const struct {
                    const char *key;
                    const char *name;
                    const char *unit;
                    const point_threshold_config_t *threshold;
                } point_defs[] = {
                    {"temperature", "Temperature", "C", &config.temperature},
                    {"humidity", "Humidity", "%", &config.humidity},
                };

                for (size_t i = 0; i < sizeof(point_defs) / sizeof(point_defs[0]); ++i) {
                    json_object *point = json_object_new_object();
                    if (!point)
                        continue;

                    const point_threshold_config_t *threshold = point_defs[i].threshold;
                    const int effective_enable = config.threshold_enabled && threshold->enable_alarm;
                    addString(point, "pointKey", point_defs[i].key);
                    addString(point, "pointName", point_defs[i].name);
                    addString(point, "unit", point_defs[i].unit);
                    addString(point, "valueType", "number");
                    json_object_object_add(point, "enable_alarm", json_object_new_boolean(effective_enable));
                    json_object_object_add(point, "enableAlarm", json_object_new_boolean(effective_enable));
                    addNullableDouble(point, "alarm_low", threshold->has_low, threshold->alarm_low);
                    addNullableDouble(point, "alarmLow", threshold->has_low, threshold->alarm_low);
                    addNullableDouble(point, "alarm_high", threshold->has_high, threshold->alarm_high);
                    addNullableDouble(point, "alarmHigh", threshold->has_high, threshold->alarm_high);
                    json_object_array_add(points, point);
                }
                json_object_object_add(dev, "points", points);
            }

            json_object_array_add(devices, dev);
            json_object_object_add(root, "devices", devices);
        } else {
            if (devices)
                json_object_put(devices);
            if (dev)
                json_object_put(dev);
        }

        const char *payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
        if (payload) {
            offline_publish_meta_t meta = {};
            meta.message_type = "threshold_config";
            meta.gateway_id = DEFAULT_GATEWAY_ID;
            meta.port_id = slot == 0 ? "port_001" : "port_002";
            meta.device_id = device.sensor->slaveId();
            meta.priority = 0;
            meta.timestamp_ms = currentTimeMs();
            (void)offline_publish_or_cache(MQTT_DEFAULT_PUBLISH_TOPIC, payload, &meta);
        }

        json_object_put(root);
    }

    void publishThresholdAlarm(int slot,
                               const SensorDevice &sensor,
                               const device_data_t &dev,
                               const ThresholdAlarmEvent &event)
    {
        if (!event.active)
            return;

        json_object *root = json_object_new_object();
        if (!root)
            return;

        addString(root, "type", "command");
        addString(root, "cmd", "emergency");
        json_object_object_add(root, "level", json_object_new_int(1));
        addString(root, "reason", event.reason);
        json_object_object_add(root, "deviceId", json_object_new_int(sensor.slaveId()));
        addString(root, "deviceType", sensor.deviceTypeName());
        addString(root, "pointKey", event.point_key);
        json_object_object_add(root, "value", json_object_new_double(event.value));
        json_object_object_add(root, "threshold", json_object_new_double(event.threshold));
        if (dev.type == DEV_SENSOR_TH) {
            json_object_object_add(root, "temp", json_object_new_double(dev.data.th.temperature));
            json_object_object_add(root, "humi", json_object_new_double(dev.data.th.humidity));
        }

        const char *payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
        if (payload) {
            (void)ipc_server_send(payload);
            offline_publish_meta_t meta = {};
            meta.message_type = "alarm_event";
            meta.gateway_id = DEFAULT_GATEWAY_ID;
            meta.port_id = slot == 0 ? "port_001" : "port_002";
            meta.device_id = sensor.slaveId();
            meta.point_key = event.point_key;
            meta.priority = 3;
            meta.has_alarm = 1;
            meta.timestamp_ms = currentTimeMs();
            (void)offline_publish_or_cache(MQTT_DEFAULT_PUBLISH_TOPIC, payload, &meta);
        }
        json_object_put(root);
    }

    void writeConfigLocked()
    {
        mkdir(kDeviceConfigDir, 0755);

        json_object *root = json_object_new_object();
        json_object *devices = json_object_new_array();
        if (!root || !devices) {
            if (root)
                json_object_put(root);
            if (devices)
                json_object_put(devices);
            return;
        }

        json_object_object_add(root, "version", json_object_new_int(1));
        json_object_object_add(root, "updated_ms", json_object_new_int64(currentTimeMs()));

        for (int slot = 0; slot < kSlotCount; ++slot) {
            PortChannel &channel = channels[slot];
            for (const ManagedDevice &device : channel.devices) {
                if (!device.sensor)
                    continue;

                json_object *item = json_object_new_object();
                if (!item)
                    continue;

                json_object_object_add(item, "slot", json_object_new_int(slot));
                addString(item, "port", channel.port.c_str());
                json_object_object_add(item, "baud", json_object_new_int(channel.baud));
                json_object_object_add(item, "slave_id", json_object_new_int(device.sensor->slaveId()));
                addString(item, "device_type", device.sensor->deviceTypeName());
                json_object_object_add(item, "poll_interval_ms", json_object_new_int(device.sensor->pollIntervalMs()));

                sensor_threshold_config_t config = {};
                if (device.sensor->thresholdConfig(&config)) {
                    json_object *threshold = thresholdConfigToJson(config);
                    if (threshold)
                        json_object_object_add(item, "threshold_config", threshold);
                }

                json_object_array_add(devices, item);
            }
        }

        json_object_object_add(root, "devices", devices);
        (void)json_object_to_file_ext(kDeviceConfigPath, root, JSON_C_TO_STRING_PRETTY);
        json_object_put(root);
    }

    void loadDevicesForSlotLocked(int slot)
    {
        if (!validSlot(slot))
            return;

        PortChannel &channel = channels[slot];
        json_object *root = json_object_from_file(kDeviceConfigPath);
        if (!root)
            return;

        json_object *devices = nullptr;
        if (!json_object_object_get_ex(root, "devices", &devices) ||
            !json_object_is_type(devices, json_type_array)) {
            json_object_put(root);
            return;
        }

        int count = json_object_array_length(devices);
        for (int i = 0; i < count; ++i) {
            json_object *item = json_object_array_get_idx(devices, i);
            if (!item)
                continue;

            json_object *v = nullptr;
            int saved_slot = -1;
            if (json_object_object_get_ex(item, "slot", &v))
                saved_slot = json_object_get_int(v);
            if (saved_slot != slot)
                continue;

            const char *saved_port = "";
            if (json_object_object_get_ex(item, "port", &v))
                saved_port = json_object_get_string(v);
            if (saved_port && saved_port[0] != '\0' && channel.port != saved_port)
                continue;

            int slave_id = 0;
            int poll_interval_ms = 0;
            const char *device_type = "unknown";
            if (json_object_object_get_ex(item, "slave_id", &v))
                slave_id = json_object_get_int(v);
            if (json_object_object_get_ex(item, "poll_interval_ms", &v))
                poll_interval_ms = json_object_get_int(v);
            if (json_object_object_get_ex(item, "device_type", &v))
                device_type = json_object_get_string(v);

            if (slave_id <= 0 || poll_interval_ms < kMinPollIntervalMs)
                continue;

            ManagedType type = parseType(device_type);
            if (type == ManagedType::Unknown)
                continue;

            auto exists = std::find_if(channel.devices.begin(), channel.devices.end(),
                                       [slave_id](const ManagedDevice &device) {
                                           return device.sensor && device.sensor->slaveId() == slave_id;
                                       });
            if (exists != channel.devices.end())
                continue;

            sensor_threshold_config_t config = {};
            sensor_threshold_config_t *config_ptr = nullptr;
            json_object *threshold = nullptr;
            if (json_object_object_get_ex(item, "threshold_config", &threshold)) {
                thresholdConfigFromJson(threshold, &config);
                config_ptr = &config;
            }

            ManagedDevice device;
            device.sensor = createSensorDevice(type, slave_id, poll_interval_ms, config_ptr);
            if (!device.sensor)
                continue;

            device.next_poll_time = std::chrono::steady_clock::now();
            channel.devices.push_back(device);
        }

        json_object_put(root);
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

    void updateLastData(int slot, int slave_id, const device_data_t &dev)
    {
        std::lock_guard<std::mutex> guard(lock);
        if (!validSlot(slot))
            return;

        PortChannel &channel = channels[slot];
        for (ManagedDevice &device : channel.devices) {
            if (device.sensor && device.sensor->slaveId() == slave_id) {
                device.last_data = dev;
                device.has_last_data = true;
                return;
            }
        }
    }

    void pollDevice(int slot, std::shared_ptr<SensorDevice> sensor)
    {
        PortChannel *channel = nullptr;
        std::unique_lock<std::mutex> io_guard;

        if (!sensor)
            return;

        {
            std::lock_guard<std::mutex> guard(lock);
            channel = &channels[slot];
            if (!channel->connected || !channel->bus)
                return;
            io_guard = std::unique_lock<std::mutex>(channel->io_lock);
        }

        device_data_t dev = {};
        int ret = -1;

        ret = sensor->read(*channel->bus, &dev);
        io_guard.unlock();

        if (ret == 0 || dev.type != DEV_UNKNOWN) {
            (void)data_publish_device_status(&dev);
            updateLastData(slot, sensor->slaveId(), dev);

            ThresholdAlarmEvent event;
            if (sensor->checkThreshold(dev, &event))
                publishThresholdAlarm(slot, *sensor, dev, event);
        }
    }

    void publishLatestStatus()
    {
        std::vector<device_data_t> latest;
        {
            std::lock_guard<std::mutex> guard(lock);
            for (const PortChannel &channel : channels) {
                for (const ManagedDevice &device : channel.devices) {
                    if (device.has_last_data)
                        latest.push_back(device.last_data);
                }
            }
        }

        for (const device_data_t &dev : latest)
            (void)data_publish_device_status(&dev);
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
        impl_->loadDevicesForSlotLocked(slot);
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

    std::vector<std::shared_ptr<SensorDevice>> due_devices;
    auto now = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> guard(impl_->lock);
        PortChannel &channel = impl_->channels[slot];
        if (!channel.connected || !channel.bus)
            return;

        for (ManagedDevice &device : channel.devices) {
            if (now < device.next_poll_time)
                continue;

            due_devices.push_back(device.sensor);
            if (device.sensor) {
                device.next_poll_time =
                    now + std::chrono::milliseconds(device.sensor->pollIntervalMs());
            }
        }
    }

    for (const std::shared_ptr<SensorDevice> &device : due_devices)
        impl_->pollDevice(slot, device);
}

void PortManager::publishLatestStatus()
{
    impl_->publishLatestStatus();
}

int PortManager::addDevice(int slot,
                           int slave_id,
                           const char *device_type,
                           int poll_interval_ms,
                           const sensor_threshold_config_t *threshold_config,
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
        setReason(reason, reason_size, "invalid_argument");
        return -1;
    }

    std::lock_guard<std::mutex> guard(impl_->lock);
    PortChannel &channel = impl_->channels[slot];
    if (!channel.connected || !channel.bus) {
        setReason(reason, reason_size, "port_not_found");
        return -1;
    }

    auto exists = std::find_if(channel.devices.begin(), channel.devices.end(),
                               [slave_id](const ManagedDevice &device) {
                                   return device.sensor && device.sensor->slaveId() == slave_id;
                               });
    if (exists != channel.devices.end()) {
        setReason(reason, reason_size, "slave_address_conflict");
        return -1;
    }

    ManagedDevice device;
    device.sensor = createSensorDevice(type, slave_id, poll_interval_ms, threshold_config);
    if (!device.sensor) {
        setReason(reason, reason_size, "unsupported_device_type");
        return -1;
    }
    device.next_poll_time = std::chrono::steady_clock::now();
    channel.devices.push_back(device);
    impl_->writeConfigLocked();
    impl_->publishThresholdConfig(slot, channel.devices.back());

    setReason(reason, reason_size, "");
    return 0;
}

int PortManager::setDeviceThreshold(int slot,
                                    int slave_id,
                                    const sensor_threshold_config_t *threshold_config,
                                    char *reason,
                                    size_t reason_size)
{
    if (!validSlot(slot) || slave_id <= 0 || !threshold_config) {
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
                               return device.sensor && device.sensor->slaveId() == slave_id;
                           });
    if (it == channel.devices.end()) {
        setReason(reason, reason_size, "device_not_found");
        return -1;
    }

    if ((*it).sensor->deviceType() != DEV_SENSOR_TH) {
        setReason(reason, reason_size, "unsupported_device_type");
        return -1;
    }

    (*it).sensor->setThresholdConfig(*threshold_config);
    impl_->writeConfigLocked();
    impl_->publishThresholdConfig(slot, *it);

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
                               return device.sensor && device.sensor->slaveId() == slave_id;
                           });
    if (it == channel.devices.end()) {
        setReason(reason, reason_size, "device_not_found");
        return -1;
    }

    channel.devices.erase(it);
    impl_->writeConfigLocked();
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
    std::shared_ptr<RelayDevice> relay;

    {
        std::lock_guard<std::mutex> guard(impl_->lock);
        channel = &impl_->channels[slot];
        if (!channel->connected || !channel->bus) {
            setReason(reason, reason_size, "port_not_connected");
            return -1;
        }

        auto it = std::find_if(channel->devices.begin(), channel->devices.end(),
                               [slave_id](const ManagedDevice &device) {
                                   return device.sensor &&
                                          device.sensor->slaveId() == slave_id &&
                                          device.sensor->deviceType() == DEV_RELAY;
                               });
        if (it == channel->devices.end()) {
            setReason(reason, reason_size, "relay_not_connected");
            return -1;
        }

        relay = std::static_pointer_cast<RelayDevice>(it->sensor);
        io_guard = std::unique_lock<std::mutex>(channel->io_lock);
    }

    if (!relay || relay->writeStates(*channel->bus, dev->data.relay.relay_states) != 0) {
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
    return g_manager.addDevice(slot, slave_id, device_type, poll_interval_ms, nullptr, reason, reason_size);
}

extern "C" int port_manager_add_device_ex(int slot,
                                           int slave_id,
                                           const char *device_type,
                                           int poll_interval_ms,
                                           const sensor_threshold_config_t *threshold_config,
                                           char *reason,
                                           size_t reason_size)
{
    return g_manager.addDevice(slot,
                               slave_id,
                               device_type,
                               poll_interval_ms,
                               threshold_config,
                               reason,
                               reason_size);
}

extern "C" int port_manager_set_device_threshold(int slot,
                                                 int slave_id,
                                                 const sensor_threshold_config_t *threshold_config,
                                                 char *reason,
                                                 size_t reason_size)
{
    return g_manager.setDeviceThreshold(slot, slave_id, threshold_config, reason, reason_size);
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

extern "C" void port_manager_publish_latest_status(void)
{
    g_manager.publishLatestStatus();
}
