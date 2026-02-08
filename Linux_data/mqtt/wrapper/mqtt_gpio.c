#include "mqtt_gpio.h"
#include "mqtt_wrapper.h"   // 用 extern g_client
#include "mqtt_log.h"
#include "mqttclient.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

/* ================= GPIO 路径 ================= */
#define GPIO_BUZZER "/sys/class/gpio/gpio19/value"
#define GPIO_LED    "/sys/class/leds/red/brightness"

/* ================= MQTT ACK Topic ================= */
#define ACK_BUZZER  "imx6ull/gpio/buzzer/ack"
#define ACK_LED     "imx6ull/gpio/led/ack"

/* ================= 全局 MQTT client ================= */
extern mqtt_client_t *g_client;

/* ================= GPIO 操作工具 ================= */
static int gpio_write(const char *path, int value)
{
    FILE *fp = fopen(path, "w");
    if (!fp) {
        MQTT_LOG_E("open %s failed: %s", path, strerror(errno));
        return -1;
    }

    if (fprintf(fp, "%d", value) < 0) {
        MQTT_LOG_E("write %s failed", path);
        fclose(fp);
        return -2;
    }

    fclose(fp);
    return 0;
}

/* ================= 发送 ACK ================= */
static void mqtt_publish_ack(const char *topic, int result, int value)
{
    if (!g_client)
        return;

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"result\":%d,\"value\":%d}", result, value);

    mqtt_message_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.qos = QOS0;
    msg.retained = 0;
    msg.payload = payload;
    msg.payloadlen = strlen(payload);

    mqtt_publish(g_client, topic, &msg);
}

/* ================= GPIO 控制处理 ================= */
static void handle_buzzer(int value)
{
    int ret = gpio_write(GPIO_BUZZER, value);
    mqtt_publish_ack(ACK_BUZZER, ret, value);
}

static void handle_led(int value)
{
    int ret = gpio_write(GPIO_LED, value);
    mqtt_publish_ack(ACK_LED, ret, value);
}

/* ================= MQTT 消息分发 ================= */
void mqtt_gpio_dispatch(const char *topic, const char *payload, int payload_len)
{
    if (!topic || !payload || payload_len <= 0)
        return;

    /* 只支持 '1' / '0' */
    int value = (payload[0] == '1') ? 1 : 0;

    MQTT_LOG_I("GPIO cmd: topic=%s value=%d", topic, value);

    if (strcmp(topic, "imx6ull/gpio/buzzer/set") == 0) {
        handle_buzzer(value);
    }
    else if (strcmp(topic, "imx6ull/gpio/led/set") == 0) {
        handle_led(value);
    }
    else {
        MQTT_LOG_W("Unknown GPIO topic: %s", topic);
    }
}
