#ifndef DATA_PUBLISH_H
#define DATA_PUBLISH_H

#include "data_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

void data_publish_device_status(const device_data_t *dev);

#ifdef __cplusplus
}
#endif

#endif
