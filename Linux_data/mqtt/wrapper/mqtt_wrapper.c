#include "mqtt_wrapper.h"

#include "mqttclient.h"
#include "mqtt_log.h"
#include "data_command.h"
#include "data_protocol.h"
#include "data_publish.h"
#include "data_telemetry.h"
#include "data_config_sync.h"
#include "OfflinePublishQueueC.h"
#include "port_manager.h"

#include <json-c/json.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define MQTT_HOST              "192.168.10.100"
#define MQTT_PORT              "1883"
#define MQTT_CLIENT_ID         "IM6ULL"
#define MQTT_QUEUE_MAX         32
#define MQTT_PAYLOAD_MAX       4096
#define MQTT_GATEWAY_HEARTBEAT_MS 10000
#define MQTT_OFFLINE_CACHE_DB_PATH "/etc/qt_object/offline_cache.db"

const char MQTT_DEFAULT_PUBLISH_TOPIC[] = "gateway/" DEFAULT_GATEWAY_ID "/up";
const char MQTT_GATEWAY_REGISTER_TOPIC[] = "gateway/register";
const char MQTT_GATEWAY_COMMAND_WILDCARD_TOPIC[] = "cmd/" DEFAULT_GATEWAY_ID "/#";

/* ================= 内部结构 ================= */

typedef struct {
    char topic[128];
    char payload[MQTT_PAYLOAD_MAX];
} mqtt_item_t;

/* ================= 内部状态 ================= */

static mqtt_client_t *g_client = NULL;
static int g_connected = 0;
static int g_subscribed = 0;

static unsigned int g_gateway_seq = 1000;
static long long g_last_gateway_heartbeat_ms = 0;
static long long g_last_offline_flush_ms = 0;

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

/* ================= 时间和序号 ================= */

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

static const char *port_id_from_slot(int slot)
{
    return slot == 0 ? "port_001" : "port_002";
}

static int copy_token(char *out, int out_size, const char *begin, size_t len)
{
    if (!out || out_size <= 0 || !begin || len >= (size_t)out_size)
        return -1;

    memcpy(out, begin, len);
    out[len] = '\0';
    return 0;
}

int mqtt_make_gateway_up_topic(char *buffer, int buffer_size)
{
    if (!buffer || buffer_size <= 0)
        return -1;
    int len = snprintf(buffer, (size_t)buffer_size, "gateway/%s/up", DEFAULT_GATEWAY_ID);
    return (len > 0 && len < buffer_size) ? 0 : -1;
}

int mqtt_make_port_up_topic(const char *port_id, char *buffer, int buffer_size)
{
    if (!buffer || buffer_size <= 0 || !port_id || port_id[0] == '\0')
        return -1;
    int len = snprintf(buffer, (size_t)buffer_size, "gateway/%s/%s/up", DEFAULT_GATEWAY_ID, port_id);
    return (len > 0 && len < buffer_size) ? 0 : -1;
}

int mqtt_make_port_up_topic_for_slot(int slot, char *buffer, int buffer_size)
{
    return mqtt_make_port_up_topic(port_id_from_slot(slot), buffer, buffer_size);
}

int mqtt_make_gateway_command_topic(char *buffer, int buffer_size)
{
    if (!buffer || buffer_size <= 0)
        return -1;
    int len = snprintf(buffer, (size_t)buffer_size, "cmd/%s/gateway", DEFAULT_GATEWAY_ID);
    return (len > 0 && len < buffer_size) ? 0 : -1;
}

int mqtt_make_port_command_topic(const char *port_id, char *buffer, int buffer_size)
{
    if (!buffer || buffer_size <= 0 || !port_id || port_id[0] == '\0')
        return -1;
    int len = snprintf(buffer, (size_t)buffer_size, "cmd/%s/%s", DEFAULT_GATEWAY_ID, port_id);
    return (len > 0 && len < buffer_size) ? 0 : -1;
}

int mqtt_parse_gateway_command_topic(const char *topic, char *gateway_id, int gateway_size)
{
    const char prefix[] = "cmd/";
    if (!topic || strncmp(topic, prefix, strlen(prefix)) != 0)
        return -1;

    const char *gateway = topic + strlen(prefix);
    const char *slash = strchr(gateway, '/');
    if (slash || gateway[0] == '\0')
        return -1;

    return copy_token(gateway_id, gateway_size, gateway, strlen(gateway));
}

int mqtt_parse_port_command_topic(const char *topic,
                                  char *gateway_id,
                                  int gateway_size,
                                  char *port_id,
                                  int port_size)
{
    const char prefix[] = "cmd/";
    if (!topic || strncmp(topic, prefix, strlen(prefix)) != 0)
        return -1;

    const char *gateway = topic + strlen(prefix);
    const char *slash = strchr(gateway, '/');
    if (!slash || slash == gateway || slash[1] == '\0' || strchr(slash + 1, '/'))
        return -1;

    if (copy_token(gateway_id, gateway_size, gateway, (size_t)(slash - gateway)) != 0)
        return -1;
    return copy_token(port_id, port_size, slash + 1, strlen(slash + 1));
}

int mqtt_parse_port_up_topic(const char *topic,
                             char *gateway_id,
                             int gateway_size,
                             char *port_id,
                             int port_size)
{
    const char prefix[] = "gateway/";
    if (!topic || strncmp(topic, prefix, strlen(prefix)) != 0)
        return -1;

    const char *gateway = topic + strlen(prefix);
    const char *gateway_end = strchr(gateway, '/');
    if (!gateway_end || gateway_end == gateway)
        return -1;

    const char *port = gateway_end + 1;
    const char *port_end = strchr(port, '/');
    if (!port_end || port_end == port || strcmp(port_end, "/up") != 0)
        return -1;

    if (copy_token(gateway_id, gateway_size, gateway, (size_t)(gateway_end - gateway)) != 0)
        return -1;
    return copy_token(port_id, port_size, port, (size_t)(port_end - port));
}

static int mqtt_payload_is_ack(const char *payload)
{
    if (!payload)
        return 0;

    struct json_object *root = json_tokener_parse(payload);
    if (!root)
        return 0;

    int is_ack = 0;
    struct json_object *type_obj = NULL;
    if (json_object_object_get_ex(root, "type", &type_obj)) {
        const char *type = json_object_get_string(type_obj);
        is_ack = type && strcmp(type, "ack") == 0;
    }

    json_object_put(root);
    return is_ack;
}

/* ================= MQTT 回调函数 ================= */

static void mqtt_message_handler(void *client, message_data_t *msg)
{
    (void)client;

    if (!msg || !msg->topic_name || !msg->message || !msg->message->payload)
        return;

    const char *topic = msg->topic_name;
    const char *payload = (const char *)msg->message->payload;
    int payloadlen = msg->message->payloadlen;

    printf("[MQTT] received topic=%s payload=%.*s\n",
           topic, payloadlen, payload);

    /*
     * 现在 Linux_data 订阅 Pc_data 下发的统一命令主题：
     *
     *   cmd/<gatewayId>/#
     *
     * 所有控制命令统一交给 data_command_process_message() 处理。
     * 不再使用旧的 imx6ull/gpio/+/set 控制方式。
     */
    char topic_gateway[64];
    char topic_port[64];
    int is_port_cmd =
        mqtt_parse_port_command_topic(topic,
                                      topic_gateway,
                                      sizeof(topic_gateway),
                                      topic_port,
                                      sizeof(topic_port)) == 0 &&
        strcmp(topic_gateway, DEFAULT_GATEWAY_ID) == 0;

    if (is_port_cmd) {
        char command[MQTT_PAYLOAD_MAX];
        int copy_len = payloadlen < (MQTT_PAYLOAD_MAX - 1)
                           ? payloadlen
                           : (MQTT_PAYLOAD_MAX - 1);

        memcpy(command, payload, copy_len);
        command[copy_len] = '\0';

        data_config_sync_handle_ack_json(command);
        if (mqtt_payload_is_ack(command))
            return;
        data_command_process_message(command);
        return;
    }

    printf("[MQTT] unknown subscribed topic: %s\n", topic);
}

/* ================= MQTT 发送 ================= */

static void mqtt_publish_one(const mqtt_item_t *item)
{
    mqtt_message_t msg;
    memset(&msg, 0, sizeof(msg));

    msg.qos = QOS0;

    /*
     * 普通 telemetry 不建议 retained=1。
     * retained=1 会让新连接的 Pc_data 收到旧数据，容易误判成实时数据。
     *
     * 如果以后你想让 gateway_status / device_config 这类状态包 retained，
     * 可以后续根据 topic 或 type 单独判断。
     */
    msg.retained = 0;

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
    printf("[MQTT] gateway register topic: %s\n", MQTT_GATEWAY_REGISTER_TOPIC);
    printf("[MQTT] command subscribe topic: %s\n", MQTT_GATEWAY_COMMAND_WILDCARD_TOPIC);

    offline_publish_queue_set_sender(mqtt_send_direct_if_connected);

    if (offline_publish_queue_init(MQTT_OFFLINE_CACHE_DB_PATH) != 0) {
        printf("[MQTT] offline publish cache init failed, continue without cache\n");
    }

    int cache_enabled = 0;
    int flush_enabled = 0;
    (void)port_manager_load_offline_cache_config(&cache_enabled, &flush_enabled);
    offline_publish_set_cache_enabled(cache_enabled);
    offline_publish_set_flush_enabled(flush_enabled);

    g_connected = 0;
    g_subscribed = 0;
    g_last_gateway_heartbeat_ms = 0;
    g_last_offline_flush_ms = 0;

    return 0;
}

/*
 * 由外部 MQTT 线程周期调用。
 *
 * 主要流程：
 * 1. 未连接时尝试连接 Broker
 * 2. 连接成功后订阅 cmd/<gatewayId>/#
 * 3. 发送网关注册包
 * 4. 发布当前端口/设备状态
 * 5. 定时发送网关心跳
 * 6. 每次从内存队列发送一条 MQTT 消息
 * 7. 定时尝试刷离线缓存
 */
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
            g_subscribed = 0;
            connect_attempts = 0;

            /*
             * 重连成功后主动上报网关注册和当前状态。
             * 这样 Pc_data 不需要等下一轮采集，也能立即知道网关在线。
             */
            data_publish_gateway_register(next_gateway_seq());
            port_manager_publish_latest_status();
            data_config_sync_publish_latest_snapshot(next_gateway_seq());

            if (offline_publish_flush_enabled()) {
                printf("[MQTT] reconnected, start offline cache flush pending=%d\n",
                       offline_publish_pending_count());
                offline_publish_flush_once();
            } else {
                printf("[MQTT] reconnected, offline cache auto flush disabled pending=%d\n",
                       offline_publish_pending_count());
            }

            g_last_offline_flush_ms = mqtt_time_ms();
            g_last_gateway_heartbeat_ms = 0;
        } else {
            if (connect_attempts == 1 || (connect_attempts % 50) == 0) {
                printf("[MQTT] connect failed broker=%s:%s ret=%d queue=%d\n",
                       MQTT_HOST, MQTT_PORT, ret, queue_count());
            }

            return;
        }
    }

    /* 2. 订阅统一命令主题 */
    if (g_connected && !g_subscribed) {
        mqtt_subscribe(g_client,
                       MQTT_GATEWAY_COMMAND_WILDCARD_TOPIC,
                       QOS0,
                       mqtt_message_handler);
        printf("[MQTT] subscribe topic: %s\n", MQTT_GATEWAY_COMMAND_WILDCARD_TOPIC);

        g_subscribed = 1;
    }

    /* 3. MQTT 心跳 */
    mqtt_keep_alive(g_client);

    /* 4. 网关应用层心跳 */
    long long now_ms = mqtt_time_ms();

    if (g_connected &&
        (g_last_gateway_heartbeat_ms == 0 ||
         now_ms - g_last_gateway_heartbeat_ms >= MQTT_GATEWAY_HEARTBEAT_MS)) {
        data_publish_gateway_heartbeat(next_gateway_seq());
        g_last_gateway_heartbeat_ms = now_ms;
    }

    /* 5. 内存队列每轮发送一条 */
    if (queue_pop(&item) == 0) {
        mqtt_publish_one(&item);
    }

    /* 6. 离线缓存定时刷出 */
    if (offline_publish_flush_enabled() &&
        now_ms - g_last_offline_flush_ms >= 200) {
        offline_publish_flush_once();
        g_last_offline_flush_ms = now_ms;
    }

    data_config_sync_tick();
}

/*
 * 业务层唯一发送接口。
 *
 * 注意：
 * 这里如果 MQTT 未连接，会直接返回 DATA_SEND_MQTT_NOT_READY。
 * 所以 data_publish.c 里面要负责：
 *
 *   mqtt_send() 失败后，把重要数据写入 offline_publish_queue。
 */
int mqtt_send(const char *topic, const char *payload)
{
    if (!topic || !payload)
        return DATA_SEND_INVALID_ARG;

    if (!g_client)
        return DATA_SEND_MQTT_NOT_READY;

    if (!g_connected)
        return DATA_SEND_MQTT_NOT_READY;

    if (queue_push(topic, payload) < 0) {
        printf("[MQTT] send queue full, drop topic=%s queue=%d/%d connected=%d broker=%s:%s\n",
               topic,
               queue_count(),
               MQTT_QUEUE_MAX - 1,
               g_connected,
               MQTT_HOST,
               MQTT_PORT);

        return DATA_SEND_MQTT_QUEUE_FULL;
    }

    return DATA_SEND_OK;
}

int mqtt_is_connected(void)
{
    return g_connected;
}

/*
 * 离线缓存模块使用的发送接口。
 *
 * offline_publish_flush_once() 会通过这个函数尝试重新发送缓存数据。
 * 如果当前 MQTT 没连接，则返回 DATA_SEND_MQTT_NOT_READY。
 */
int mqtt_send_direct_if_connected(const char *topic, const char *payload)
{
    if (!topic || !payload)
        return DATA_SEND_INVALID_ARG;

    if (!g_client)
        return DATA_SEND_MQTT_NOT_READY;

    if (!g_connected)
        return DATA_SEND_MQTT_NOT_READY;

    if (queue_push(topic, payload) < 0) {
        printf("[MQTT] direct send queue full topic=%s queue=%d/%d\n",
               topic,
               queue_count(),
               MQTT_QUEUE_MAX - 1);

        return DATA_SEND_MQTT_QUEUE_FULL;
    }

    return DATA_SEND_OK;
}

void mqtt_set_offline_cache_enabled(int enabled)
{
    offline_publish_set_cache_enabled(enabled);
}

int mqtt_offline_cache_enabled(void)
{
    return offline_publish_cache_enabled();
}

void mqtt_set_offline_cache_flush_enabled(int enabled)
{
    offline_publish_set_flush_enabled(enabled);
}

int mqtt_offline_cache_flush_enabled(void)
{
    return offline_publish_flush_enabled();
}

int mqtt_offline_cache_pending_count(void)
{
    return offline_publish_pending_count();
}

int mqtt_clear_offline_cache(void)
{
    return offline_publish_clear_pending();
}

int mqtt_flush_offline_cache_once(void)
{
    if (!offline_publish_flush_enabled())
        return DATA_SEND_OFFLINE_CACHE_DISABLED;

    if (offline_publish_pending_count() <= 0)
        return DATA_SEND_OK;

    if (!mqtt_is_connected())
        return DATA_SEND_MQTT_NOT_READY;

    const int old_flush_enabled = offline_publish_flush_enabled();
    offline_publish_set_flush_enabled(1);
    offline_publish_flush_once();
    offline_publish_set_flush_enabled(old_flush_enabled);

    return DATA_SEND_OK;
}
