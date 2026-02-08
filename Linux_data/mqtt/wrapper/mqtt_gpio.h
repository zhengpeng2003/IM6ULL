#ifndef MQTT_GPIO_H
#define MQTT_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

/* 处理接收到的 MQTT 消息，解析 topic 和 payload，控制 GPIO */
void mqtt_gpio_dispatch(const char *topic,
                        const char *payload,
                        int payload_len);

#ifdef __cplusplus
}
#endif

#endif /* MQTT_GPIO_H */
