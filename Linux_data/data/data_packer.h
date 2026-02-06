#ifndef DATA_PACKER_H
#define DATA_PACKER_H

#include "data_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif


int data_pack_to_json(const data_pack_t *pack,
                      char *buf,
                      int buf_size);
data_pack_t data_pack_single(const device_data_t *dev);

#ifdef __cplusplus
}
#endif

#endif

