#include "mqtt_wrapper.h"
#include "mqttclient.h"
#include "mqtt_log.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#define MQTT_HOST        "broker.emqx.io"
#define MQTT_PORT        "1883"
#define MQTT_QUEUE_MAX   32
#define MQTT_PAYLOAD_MAX 512

/* ================= 内部结构 ================= */

typedef struct {
    char topic[128];
    char payload[MQTT_PAYLOAD_MAX];
} mqtt_item_t;

/* ================= 内部状态 ================= */

static mqtt_client_t *g_client = NULL;
static int g_connected = 0;

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

static int queue_push(const char *topic, const char *payload)
{
    pthread_mutex_lock(&g_mutex);

    if (queue_full()) {
        pthread_mutex_unlock(&g_mutex);
        return -1;
    }

    strncpy(g_queue[g_tail].topic, topic, sizeof(g_queue[g_tail].topic) - 1);
    strncpy(g_queue[g_tail].payload, payload, sizeof(g_queue[g_tail].payload) - 1);

    g_tail = (g_tail + 1) % MQTT_QUEUE_MAX;

    pthread_mutex_unlock(&g_mutex);
    return 0;
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

/* ================= MQTT 发送 ================= */

static void mqtt_publish_one(const mqtt_item_t *item)
{
    mqtt_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.qos = QOS0;
    msg.retained = 1;
    msg.payload = (void *)item->payload;
    msg.payloadlen = strlen(item->payload);

    mqtt_publish(g_client, item->topic, &msg);
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

    char cid[64];
    snprintf(cid, sizeof(cid), "imx6ull-%d", getpid());
    mqtt_set_client_id(g_client, cid);
    mqtt_set_clean_session(g_client, 1);

    g_connected = 0;
    return 0;
}

/* ⭐⭐ 由外部线程周期调用 ⭐⭐ */
void mqtt_poll(void)
{
    mqtt_item_t item;

    if (!g_client)
        return;

    /* 1. 确保连接 */
    if (!g_connected) {
        if (mqtt_connect(g_client) == 0) {
            printf("[MQTT] connected\n");
            g_connected = 1;
        } else {
            return;   // 本轮不再继续
        }
    }

    /* 2. 心跳 */
    mqtt_keep_alive(g_client);

    /* 3. 发送一条 */
    if (queue_pop(&item) == 0) {
        mqtt_publish_one(&item);
    }
}

/* 业务层唯一发送接口 */
int mqtt_send(const char *topic, const char *payload)
{
    if (!g_client || !topic || !payload)
        return -1;

    /* 队列满，直接返回失败 */
    if (queue_push(topic, payload) < 0) {
        printf("[MQTT] send queue full, drop\n");
        return -1;
    }
    return 0;   // ✅ 表示：已接收发送请求
}

