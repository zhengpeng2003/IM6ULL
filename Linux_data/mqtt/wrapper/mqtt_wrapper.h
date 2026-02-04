#ifndef MQTT_WRAPPER_H
#define MQTT_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif
/* 初始化并连接 MQTT broker */
int mqtt_wrapper_init(void);

/* 发布 JSON 数据 */
int mqtt_wrapper_publish(const char *topic, const char *json_payload);

/* 断开连接并清�*/
void mqtt_wrapper_deinit(void);

#ifdef __cplusplus
}
#endif

#endif
