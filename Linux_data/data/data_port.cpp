#include "data_port.h"

#include <stdio.h>

int data_port_pack_ports_json(const std::vector<std::string> &ports,
                              char *buf,
                              int buf_size)
{
    if (!buf || buf_size <= 0)
        return -1;

    int off = snprintf(buf, buf_size, "{\"type\":\"ports\",\"ports\":[");
    for (size_t i = 0; i < ports.size() && off < buf_size; ++i) {
        off += snprintf(buf + off, buf_size - off,
                        "%s\"%s\"",
                        i == 0 ? "" : ",",
                        ports[i].c_str());
    }
    off += snprintf(buf + off, buf_size - off, "]}");

    if (off >= buf_size)
        return -1;

    return off;
}

int data_port_pack_status_json(int slot,
                               const char *port,
                               const char *device_type,
                               int baud,
                               bool connected,
                               const char *message,
                               char *buf,
                               int buf_size)
{
    if (!buf || buf_size <= 0)
        return -1;

    int len = snprintf(buf, buf_size,
        "{\"type\":\"port_status\",\"slot\":%d,\"port\":\"%s\","
        "\"device_type\":\"%s\",\"baud\":%d,\"connected\":%s,"
        "\"message\":\"%s\"}",
        slot,
        port ? port : "",
        device_type ? device_type : "unknown",
        baud,
        connected ? "true" : "false",
        message ? message : "");

    if (len >= buf_size)
        return -1;

    return len;
}
