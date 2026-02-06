#pragma once

#ifdef __cplusplus
extern "C" {
#endif

int  mqtt_init(void);
void mqtt_poll(void);
int  mqtt_send(const char *topic, const char *payload);

#ifdef __cplusplus
}
#endif

