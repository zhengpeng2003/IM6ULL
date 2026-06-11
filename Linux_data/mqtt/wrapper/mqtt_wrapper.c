#include "mqtt_wrapper.h"
#include "mqtt_gpio.h"
#include "mqttclient.h"
#include "mqtt_log.h"
#include "data_command.h"
#include "data_protocol.h"
#include "data_publish.h"
#include "data_telemetry.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define MQTT_HOST              "192.168.10.100"
#define MQTT_PORT              "1883"
#define MQTT_CLIENT_ID         "IM6ULL"
#define MQTT_SUB_TOPIC_DATA    "imx6ull/device/data"
#define MQTT_SUB_TOPIC_GPIO    "imx6ull/gpio/+/set"
#define MQTT_QUEUE_MAX         32
#define MQTT_PAYLOAD_MAX       4096
#define MQTT_GATEWAY_HEARTBEAT_MS 10000

const char MQTT_DEFAULT_PUBLISH_TOPIC[] = "pc_data/telemetry/test";

/* ================= 内部结构 ================= */

typedef struct {
    char topic[128];
    char payload[MQTT_PAYLOAD_MAX];
} mqtt_item_t;

/* ================= 内部状态 ================= */

static mqtt_client_t *g_client = NULL;
static int g_connected = 0;
static int g_subscribed = 0;  // 新增：订阅状态标志
static unsigned int g_gateway_seq = 1000;
static long long g_last_gateway_heartbeat_ms = 0;

/* 发送队列 */
static mqtt_item_t g_queue[MQTT_QUEUE_MAX];
static int g_head = 0;
static int g_tail = 0;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ================= 队列工具 ================= */

static int queue_empty(void)
{
    return g_head == g_tail;
}

static int queue_full(void)
{
    return ((g_tail + 1) % MQTT_QUEUE_MAX) == g_head;
}

static int queue_count_locked(void)
{
    if (g_tail >= g_head)
        return g_tail - g_head;
    return MQTT_QUEUE_MAX - g_head + g_tail;
}

static int queue_count(void)
{
    int count;

    pthread_mutex_lock(&g_mutex);
    count = queue_count_locked();
    pthread_mutex_unlock(&g_mutex);

    return count;
}

static int queue_push(const char *topic, const char *payload)
{
    pthread_mutex_lock(&g_mutex);

    if (queue_full()) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    strncpy(g_queue[g_tail].topic, topic, sizeof(g_queue[g_tail].topic) - 1);
    g_queue[g_tail].topic[sizeof(g_queue[g_tail].topic) - 1] = '\0';
    strncpy(g_queue[g_tail].payload, payload, sizeof(g_queue[g_tail].payload) - 1);
    g_queue[g_tail].payload[sizeof(g_queue[g_tail].payload) - 1] = '\0';

    g_tail = (g_tail + 1) % MQTT_QUEUE_MAX;

    pthread_mutex_unlock(&g_mutex);
    return 0;
}

static long long mqtt_time_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static unsigned int next_gateway_seq(void)
{
    return ++g_gateway_seq;
}

static int queue_pop(mqtt_item_t *out)
{
    pthread_mutex_lock(&g_mutex);

    if (queue_empty()) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    *out = g_queue[g_head];
    g_head = (g_head + 1) % MQTT_QUEUE_MAX;

    pthread_mutex_unlock(&g_mutex);
    return 0;
}

/* ================= MQTT 回调函数 ================= */

// 新增：消息接收回调函数
static void mqtt_message_handler(void *client, message_data_t *msg)
{
        (void)client;
    if (!msg || !msg->topic_name || !msg->message || !msg->message->payload)
        return;

    const char *topic = msg->topic_name;
    const char *payload = (const char*)msg->message->payload;
    int payloadlen = msg->message->payloadlen;

    printf("[MQTT] Received: topic=%s, payload=%.*s\n", 
           topic, payloadlen, payload);

    /* ⭐⭐ 消息分类处理 ⭐⭐ */
    if (strstr(topic, "/gpio/")) {
        /* GPIO 控制命令 */
        mqtt_gpio_dispatch(topic, payload, payloadlen);
    }
    else if (strstr(topic, "/device/data")) {
        /* 温度/传感器数据 - 这里处理或忽略 */
        printf("[MQTT] Sensor data received, skip GPIO dispatch\n");
        // 如果需要处理传感器数据，在这里添加逻辑
        // 或者转发给其他模块
    }
    else if (strcmp(topic, "cmd/" DEFAULT_GATEWAY_ID) == 0) {
        char command[MQTT_PAYLOAD_MAX];
        int copy_len = payloadlen < (MQTT_PAYLOAD_MAX - 1) ? payloadlen : (MQTT_PAYLOAD_MAX - 1);
        memcpy(command, payload, copy_len);
        command[copy_len] = '\0';
        data_command_process_message(command);
    }
    else {
        printf("[MQTT] Unknown topic: %s\n", topic);
    }
}

/* ================= MQTT 发送 ================= */

static void mqtt_publish_one(const mqtt_item_t *item)
{
    mqtt_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.qos = QOS0;
    msg.retained = 1;
    msg.payload = (void *)item->payload;
    msg.payloadlen = strlen(item->payload);

    printf("[MQTT] publish topic=%s payload_len=%d\n",
           item->topic, msg.payloadlen);
    mqtt_publish(g_client, item->topic, &msg);
    printf("[MQTT] publish requested topic=%s\n", item->topic);
}

/* ================= 对外 API ================= */

int mqtt_init(void)
{
    mqtt_log_init();

    g_client = mqtt_lease();
    if (!g_client) {
        printf("[MQTT] lease failed\n");
        return -1;
    }

    mqtt_set_host(g_client, MQTT_HOST);
    mqtt_set_port(g_client, MQTT_PORT);

    mqtt_set_client_id(g_client, MQTT_CLIENT_ID);
    mqtt_set_clean_session(g_client, 1);

    printf("[MQTT] default broker: %s:%s\n", MQTT_HOST, MQTT_PORT);
    printf("[MQTT] client id: %s\n", MQTT_CLIENT_ID);
    printf("[MQTT] default publish topic: %s\n", MQTT_DEFAULT_PUBLISH_TOPIC);
    printf("[MQTT] default subscribe topic: %s\n", MQTT_SUB_TOPIC_DATA);
    printf("[MQTT] default subscribe topic: %s\n", MQTT_SUB_TOPIC_GPIO);

    g_connected = 0;
    g_subscribed = 0;  // 初始化订阅状态
    return 0;
}

/* ⭐⭐ 由外部线程周期调用 ⭐⭐ */
void mqtt_poll(void)
{
    mqtt_item_t item;
    static unsigned int connect_attempts = 0;

    if (!g_client)
        return;

    /* 1. 确保连接 */
    if (!g_connected) {
        int ret;
        connect_attempts++;
        if (connect_attempts == 1 || (connect_attempts % 50) == 0) {
            printf("[MQTT] connecting broker=%s:%s attempt=%u queue=%d\n",
                   MQTT_HOST, MQTT_PORT, connect_attempts, queue_count());
        }

        ret = mqtt_connect(g_client);
        if (ret == 0) {
            printf("[MQTT] connected broker=%s:%s after_attempts=%u\n",
                   MQTT_HOST, MQTT_PORT, connect_attempts);
            g_connected = 1;
            connect_attempts = 0;
            g_subscribed = 0;
            data_publish_gateway_register(next_gateway_seq());
            g_last_gateway_heartbeat_ms = 0;
        } else {
            if (connect_attempts == 1 || (connect_attempts % 50) == 0) {
                printf("[MQTT] connect failed broker=%s:%s ret=%d queue=%d\n",
                       MQTT_HOST, MQTT_PORT, ret, queue_count());
            }
            return;   // 本轮不再继续
        }
    }

    /* 2. 订阅主题（修改：同时订阅数据上报和控制命令） */
    if (g_connected && !g_subscribed) {
        mqtt_subscribe(g_client, MQTT_SUB_TOPIC_DATA, QOS0, mqtt_message_handler);
        printf("[MQTT] subscribe topic: %s\n", MQTT_SUB_TOPIC_DATA);

        mqtt_subscribe(g_client, MQTT_SUB_TOPIC_GPIO, QOS0, mqtt_message_handler);
        printf("[MQTT] subscribe topic: %s\n", MQTT_SUB_TOPIC_GPIO);

        mqtt_subscribe(g_client, "cmd/" DEFAULT_GATEWAY_ID, QOS0, mqtt_message_handler);
        printf("[MQTT] subscribe topic: cmd/%s\n", DEFAULT_GATEWAY_ID);

        g_subscribed = 1;
    }

    /* 3. 心跳 */
    mqtt_keep_alive(g_client);

    long long now_ms = mqtt_time_ms();
    if (g_connected &&
        (g_last_gateway_heartbeat_ms == 0 ||
         now_ms - g_last_gateway_heartbeat_ms >= MQTT_GATEWAY_HEARTBEAT_MS)) {
        data_publish_gateway_heartbeat(next_gateway_seq());
        g_last_gateway_heartbeat_ms = now_ms;
    }

    /* 4. 发送一条 */
    if (queue_pop(&item) == 0) {
        mqtt_publish_one(&item);
    }
}

/* 业务层唯一发送接口 */
int mqtt_send(const char *topic, const char *payload)
{
    if (!topic || !payload)
        return DATA_SEND_INVALID_ARG;

    if (!g_client)
        return DATA_SEND_MQTT_NOT_READY;

    /* 队列满，直接返回失败 */
    if (queue_push(topic, payload) < 0) {
        printf("[MQTT] send queue full, drop topic=%s queue=%d/%d connected=%d broker=%s:%s\n",
               topic, queue_count(), MQTT_QUEUE_MAX - 1, g_connected,
               MQTT_HOST, MQTT_PORT);
        return DATA_SEND_MQTT_QUEUE_FULL;
    }
    return DATA_SEND_OK;   // 表示：已接收发送请求
}
