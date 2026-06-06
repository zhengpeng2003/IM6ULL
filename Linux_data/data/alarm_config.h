#ifndef ALARM_CONFIG_H
#define ALARM_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

void alarm_config_init(void);
void alarm_config_get(float *temp_high, float *humi_high);
int alarm_config_set(float temp_high, float humi_high, char *reason, int reason_size);
void alarm_config_check_sensor(float temp, float humi);

#ifdef __cplusplus
}
#endif

#endif
