#ifndef DATA_PORT_H
#define DATA_PORT_H

#include <string>
#include <vector>

int data_port_pack_ports_json(const std::vector<std::string> &ports,
                              char *buf,
                              int buf_size);

int data_port_pack_status_json(int slot,
                               const char *port,
                               const char *device_type,
                               int baud,
                               bool connected,
                               const char *message,
                               char *buf,
                               int buf_size);

#endif
