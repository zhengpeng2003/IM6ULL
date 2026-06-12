#pragma once

#include "data_protocol.h"
#include <stddef.h>

#ifdef __cplusplus

#include <memory>
#include <string>
#include <vector>

class ModbusMaster;

class PortManager {
public:
    PortManager();
    ~PortManager();

    static std::vector<std::string> scanAvailablePorts();

    int connectPort(int slot,
                    const char *port,
                    int baud,
                    char *reason,
                    size_t reason_size);
    int disconnectPort(int slot, char *reason, size_t reason_size);
    void pollSlot(int slot);
    int addDevice(int slot,
                  int slave_id,
                  const char *device_type,
                  int poll_interval_ms,
                  const sensor_threshold_config_t *threshold_config,
                  char *reason,
                  size_t reason_size);
    int setDeviceThreshold(int slot,
                           int slave_id,
                           const sensor_threshold_config_t *threshold_config,
                           char *reason,
                           size_t reason_size);
    int removeDevice(int slot,
                     int slave_id,
                     char *reason,
                     size_t reason_size);
    int handleRelay(int slot,
                    int slave_id,
                    const device_data_t *dev,
                    char *reason,
                    size_t reason_size);
    int exportConfigSnapshotJson(uint32_t seq,
                                 const char *gateway_id,
                                 const char *target_json,
                                 char *buffer,
                                 size_t buffer_size);
    void publishLatestStatus();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

#endif

#ifdef __cplusplus
extern "C" {
#endif

void port_manager_scan_ports(uint32_t seq, const char *cmd);
int port_manager_connect(int slot,
                         const char *port,
                         int baud,
                         char *reason,
                         size_t reason_size);
int port_manager_disconnect(int slot, char *reason, size_t reason_size);
void port_manager_poll_slot(int slot);
int port_manager_add_device(int slot,
                            int slave_id,
                            const char *device_type,
                            int poll_interval_ms,
                            char *reason,
                            size_t reason_size);
int port_manager_add_device_ex(int slot,
                               int slave_id,
                               const char *device_type,
                               int poll_interval_ms,
                               const sensor_threshold_config_t *threshold_config,
                               char *reason,
                               size_t reason_size);
int port_manager_set_device_threshold(int slot,
                                      int slave_id,
                                      const sensor_threshold_config_t *threshold_config,
                                      char *reason,
                                      size_t reason_size);
int port_manager_remove_device(int slot,
                               int slave_id,
                               char *reason,
                               size_t reason_size);
int port_manager_handle_relay(int slot,
                              int slave_id,
                              const device_data_t *dev,
                              char *reason,
                              size_t reason_size);
int port_manager_export_config_snapshot(uint32_t seq,
                                        const char *gateway_id,
                                        const char *target_json,
                                        char *buffer,
                                        size_t buffer_size);
void port_manager_publish_latest_status(void);

#ifdef __cplusplus
}
#endif
