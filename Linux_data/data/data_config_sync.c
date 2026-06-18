#include "data_config_sync.h"

#include "data_telemetry.h"
#include "ipc_server.h"
#include "mqtt_wrapper.h"
#include "port_manager.h"
#include "OfflinePublishQueueC.h"

#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define CONFIG_SYNC_PAYLOAD_MAX 65536
#define CONFIG_SYNC_REASON_MAX 128
#define CONFIG_SYNC_ACK_TIMEOUT_MS 5000
#define CONFIG_SYNC_RETRY_INTERVAL_MS 8000
#define CONFIG_SYNC_MAX_FAST_RETRY_COUNT 3

typedef struct {
    uint32_t config_version;
    uint32_t last_acked_config_version;
    uint32_t pending_seq;
    char status[24];
    char reason[CONFIG_SYNC_REASON_MAX];
    char message[CONFIG_SYNC_REASON_MAX];
    int retry_count;
    int retryable;
    int64_t last_send_time_ms;
    int64_t last_ack_time_ms;
    char pending_cmd[MAX_CMD_NAME_LEN];
    char pending_port_id[32];
    char pending_snapshot[CONFIG_SYNC_PAYLOAD_MAX];
    int last_acked_snapshot;
} config_sync_state_t;

static config_sync_state_t g_sync = {
    0, 0, 0, "idle", "", "", 0, 1, 0, 0, "", "", "", 0
};
static pthread_mutex_t g_sync_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_sync_seq = 50000;

static int64_t sync_time_ms(void)
{
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0)
        return (int64_t)time(NULL) * 1000;
    return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void copy_text(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0)
        return;
    if (!src)
        src = "";
    snprintf(dst, dst_size, "%s", src);
}

uint32_t data_config_sync_next_seq(void)
{
    pthread_mutex_lock(&g_sync_lock);
    uint32_t seq = ++g_sync_seq;
    pthread_mutex_unlock(&g_sync_lock);
    return seq;
}

static uint32_t next_seq_locked(void)
{
    return ++g_sync_seq;
}

static void publish_state_locked(void)
{
    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    json_object_object_add(root, "type", json_object_new_string("config_sync_state"));
    json_object_object_add(root, "configVersion", json_object_new_int64(g_sync.config_version));
    json_object_object_add(root, "lastAckedConfigVersion", json_object_new_int64(g_sync.last_acked_config_version));
    json_object_object_add(root, "pendingSeq", json_object_new_int64(g_sync.pending_seq));
    json_object_object_add(root, "status", json_object_new_string(g_sync.status));
    json_object_object_add(root, "reason", json_object_new_string(g_sync.reason));
    json_object_object_add(root, "message", json_object_new_string(g_sync.message));
    json_object_object_add(root, "retryCount", json_object_new_int(g_sync.retry_count));
    json_object_object_add(root, "retryable", json_object_new_boolean(g_sync.retryable));
    json_object_object_add(root, "lastSendTimeMs", json_object_new_int64(g_sync.last_send_time_ms));
    json_object_object_add(root, "lastAckTimeMs", json_object_new_int64(g_sync.last_ack_time_ms));

    (void)ipc_server_send(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    json_object_put(root);
}

static int is_snapshot_cmd(const char *cmd)
{
    return cmd &&
           (strcmp(cmd, "config_snapshot") == 0 ||
            strcmp(cmd, "device_config_snapshot") == 0);
}

int data_config_sync_mark_sent(uint32_t seq,
                               const char *cmd,
                               const char *payload,
                               const char *port_id)
{
    if (!cmd || !payload || payload[0] == '\0')
        return DATA_SEND_INVALID_ARG;

    pthread_mutex_lock(&g_sync_lock);

    const int snapshot_cmd = is_snapshot_cmd(cmd);
    const int keep_existing_snapshot = !snapshot_cmd && g_sync.pending_snapshot[0] != '\0';

    if (snapshot_cmd)
        g_sync.config_version++;

    if (g_sync.config_version == 0)
        g_sync.config_version = 1;

    if (!keep_existing_snapshot) {
        g_sync.pending_seq = seq;
        copy_text(g_sync.pending_cmd, sizeof(g_sync.pending_cmd), cmd);
        copy_text(g_sync.pending_port_id, sizeof(g_sync.pending_port_id), port_id);
        copy_text(g_sync.pending_snapshot, sizeof(g_sync.pending_snapshot), payload);
        g_sync.retry_count = 0;
        g_sync.retryable = 1;
        g_sync.last_send_time_ms = sync_time_ms();
    }
    copy_text(g_sync.reason, sizeof(g_sync.reason), "");
    if (mqtt_is_connected()) {
        copy_text(g_sync.status, sizeof(g_sync.status), "syncing");
        copy_text(g_sync.message, sizeof(g_sync.message), "注册表同步中，等待 Pc_data 确认");
    } else {
        copy_text(g_sync.status, sizeof(g_sync.status), "offline");
        copy_text(g_sync.message, sizeof(g_sync.message), "MQTT 未连接，注册表仅保存在本地");
    }

    publish_state_locked();
    pthread_mutex_unlock(&g_sync_lock);
    return DATA_SEND_OK;
}

static int send_pending_locked(uint32_t seq)
{
    if (g_sync.pending_snapshot[0] == '\0')
        return DATA_SEND_INVALID_ARG;

    struct json_object *root = json_tokener_parse(g_sync.pending_snapshot);
    if (!root)
        return DATA_SEND_JSON_ERROR;

    json_object_object_del(root, "seq");
    json_object_object_add(root, "seq", json_object_new_int64(seq));
    json_object_object_del(root, "configVersion");
    json_object_object_add(root, "configVersion", json_object_new_int64(g_sync.config_version));

    const char *json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!json) {
        json_object_put(root);
        return DATA_SEND_JSON_ERROR;
    }

    copy_text(g_sync.pending_snapshot, sizeof(g_sync.pending_snapshot), json);
    g_sync.pending_seq = seq;
    g_sync.last_send_time_ms = sync_time_ms();

    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = g_sync.pending_cmd[0] ? g_sync.pending_cmd : "config_snapshot";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.port_id = g_sync.pending_port_id[0] ? g_sync.pending_port_id : DEFAULT_PORT_ID;
    meta.priority = 0;
    meta.timestamp_ms = g_sync.last_send_time_ms;

    int ret = offline_publish_or_cache(MQTT_GATEWAY_REGISTER_TOPIC, json, &meta);
    json_object_put(root);
    return ret;
}

int data_config_sync_publish_latest_snapshot(uint32_t seq)
{
    char snapshot[CONFIG_SYNC_PAYLOAD_MAX];
    if (port_manager_export_config_snapshot(seq, DEFAULT_GATEWAY_ID, "", snapshot, sizeof(snapshot)) != 0)
        return DATA_SEND_JSON_ERROR;

    struct json_object *root = json_tokener_parse(snapshot);
    if (!root)
        return DATA_SEND_JSON_ERROR;

    pthread_mutex_lock(&g_sync_lock);
    g_sync.config_version++;
    if (g_sync.config_version == 0)
        g_sync.config_version = 1;
    json_object_object_del(root, "configVersion");
    json_object_object_add(root, "configVersion", json_object_new_int64(g_sync.config_version));
    const char *json = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    if (!json) {
        pthread_mutex_unlock(&g_sync_lock);
        json_object_put(root);
        return DATA_SEND_JSON_ERROR;
    }
    copy_text(g_sync.pending_snapshot, sizeof(g_sync.pending_snapshot), json);
    copy_text(g_sync.pending_cmd, sizeof(g_sync.pending_cmd), "config_snapshot");
    copy_text(g_sync.pending_port_id, sizeof(g_sync.pending_port_id), DEFAULT_PORT_ID);
    g_sync.pending_seq = seq;
    g_sync.retry_count = 0;
    g_sync.retryable = 1;
    g_sync.last_send_time_ms = sync_time_ms();
    copy_text(g_sync.reason, sizeof(g_sync.reason), "");
    copy_text(g_sync.status, sizeof(g_sync.status), mqtt_is_connected() ? "syncing" : "offline");
    copy_text(g_sync.message, sizeof(g_sync.message),
              mqtt_is_connected() ? "注册表同步中，等待 Pc_data 确认" : "MQTT 未连接，注册表仅保存在本地");

    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = "config_snapshot";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.port_id = DEFAULT_PORT_ID;
    meta.priority = 0;
    meta.timestamp_ms = g_sync.last_send_time_ms;
    int ret = offline_publish_or_cache(MQTT_GATEWAY_REGISTER_TOPIC, json, &meta);
    publish_state_locked();
    pthread_mutex_unlock(&g_sync_lock);
    json_object_put(root);
    return ret;
}

static int string_in_register_cmds(const char *cmd)
{
    return cmd &&
           (strcmp(cmd, "gateway_register") == 0 ||
            strcmp(cmd, "port_register") == 0 ||
            strcmp(cmd, "port_status") == 0 ||
            strcmp(cmd, "config_snapshot") == 0 ||
            strcmp(cmd, "device_config_snapshot") == 0);
}

void data_config_sync_handle_ack_json(const char *payload)
{
    if (!payload)
        return;

    struct json_object *root = json_tokener_parse(payload);
    if (!root)
        return;

    struct json_object *v = NULL;
    const char *type = "";
    const char *cmd = "";
    const char *status = "";
    const char *reason = "";
    const char *message = "";
    int ok = 0;
    int retryable = 1;
    uint32_t seq = 0;
    uint32_t config_version = 0;

    if (json_object_object_get_ex(root, "type", &v))
        type = json_object_get_string(v);
    if (!type || strcmp(type, "ack") != 0) {
        json_object_put(root);
        return;
    }
    if (json_object_object_get_ex(root, "cmd", &v))
        cmd = json_object_get_string(v);
    if (!string_in_register_cmds(cmd)) {
        json_object_put(root);
        return;
    }
    if (json_object_object_get_ex(root, "seq", &v))
        seq = (uint32_t)json_object_get_int64(v);
    if (json_object_object_get_ex(root, "configVersion", &v))
        config_version = (uint32_t)json_object_get_int64(v);
    if (json_object_object_get_ex(root, "ok", &v))
        ok = json_object_get_boolean(v);
    if (json_object_object_get_ex(root, "status", &v))
        status = json_object_get_string(v);
    if (json_object_object_get_ex(root, "reason", &v))
        reason = json_object_get_string(v);
    if (json_object_object_get_ex(root, "message", &v))
        message = json_object_get_string(v);
    if (json_object_object_get_ex(root, "retryable", &v))
        retryable = json_object_get_boolean(v);

    pthread_mutex_lock(&g_sync_lock);
    const int snapshot_ack = is_snapshot_cmd(cmd);
    if (snapshot_ack && seq != 0 && g_sync.pending_seq != 0 && seq != g_sync.pending_seq) {
        pthread_mutex_unlock(&g_sync_lock);
        json_object_put(root);
        return;
    }

    g_sync.last_ack_time_ms = sync_time_ms();
    g_sync.retryable = retryable;
    copy_text(g_sync.reason, sizeof(g_sync.reason), reason);
    copy_text(g_sync.message, sizeof(g_sync.message), message);

    if (ok) {
        g_sync.retry_count = 0;
        copy_text(g_sync.reason, sizeof(g_sync.reason), "");
        if (snapshot_ack) {
            g_sync.last_acked_config_version = config_version > 0 ? config_version : g_sync.config_version;
            g_sync.last_acked_snapshot = 1;
            g_sync.pending_seq = 0;
            g_sync.pending_snapshot[0] = '\0';
            copy_text(g_sync.status, sizeof(g_sync.status), "success");
            copy_text(g_sync.message, sizeof(g_sync.message), "注册表已同步");
        } else if (g_sync.pending_snapshot[0] != '\0') {
            copy_text(g_sync.status, sizeof(g_sync.status), "syncing");
            copy_text(g_sync.message, sizeof(g_sync.message), "注册表同步中，等待 Pc_data 确认");
        } else {
            copy_text(g_sync.status, sizeof(g_sync.status), "success");
            copy_text(g_sync.message, sizeof(g_sync.message), "注册类数据已确认");
        }
    } else {
        copy_text(g_sync.status,
                  sizeof(g_sync.status),
                  status && strcmp(status, "partial_failed") == 0 ? "partial_failed" : "failed");
        if (!message || message[0] == '\0')
            copy_text(g_sync.message, sizeof(g_sync.message), reason);
    }

    publish_state_locked();
    pthread_mutex_unlock(&g_sync_lock);
    json_object_put(root);
}

void data_config_sync_tick(void)
{
    pthread_mutex_lock(&g_sync_lock);
    const int has_pending = g_sync.pending_snapshot[0] != '\0' && g_sync.pending_seq != 0;
    const int64_t now = sync_time_ms();
    if (!has_pending) {
        pthread_mutex_unlock(&g_sync_lock);
        return;
    }

    if (!mqtt_is_connected()) {
        if (strcmp(g_sync.status, "offline") != 0) {
            copy_text(g_sync.status, sizeof(g_sync.status), "offline");
            copy_text(g_sync.message, sizeof(g_sync.message), "MQTT 未连接，注册表仅保存在本地");
            publish_state_locked();
        }
        pthread_mutex_unlock(&g_sync_lock);
        return;
    }

    if (!g_sync.retryable) {
        pthread_mutex_unlock(&g_sync_lock);
        return;
    }

    if (now - g_sync.last_send_time_ms < CONFIG_SYNC_ACK_TIMEOUT_MS) {
        pthread_mutex_unlock(&g_sync_lock);
        return;
    }

    if (now - g_sync.last_send_time_ms < CONFIG_SYNC_RETRY_INTERVAL_MS) {
        if (strcmp(g_sync.status, "timeout") != 0) {
            copy_text(g_sync.status, sizeof(g_sync.status), "timeout");
            copy_text(g_sync.message, sizeof(g_sync.message), "未收到 Pc_data 确认，正在重试");
            publish_state_locked();
        }
        pthread_mutex_unlock(&g_sync_lock);
        return;
    }

    g_sync.retry_count++;
    uint32_t seq = next_seq_locked();
    if (g_sync.retry_count > CONFIG_SYNC_MAX_FAST_RETRY_COUNT) {
        copy_text(g_sync.status, sizeof(g_sync.status), "retrying");
        copy_text(g_sync.message, sizeof(g_sync.message), "Pc_data 长时间无响应，请检查 Pc_data / MQTT / topic 配置");
    } else {
        copy_text(g_sync.status, sizeof(g_sync.status), "timeout");
        copy_text(g_sync.message, sizeof(g_sync.message), "未收到 Pc_data 确认，正在重试");
    }
    (void)send_pending_locked(seq);
    publish_state_locked();
    pthread_mutex_unlock(&g_sync_lock);
}

int data_config_sync_telemetry_allowed(int slot, const device_data_t *dev)
{
    (void)slot;
    if (!dev)
        return 0;

    pthread_mutex_lock(&g_sync_lock);
    int allowed = g_sync.last_acked_config_version > 0 &&
                  g_sync.last_acked_config_version == g_sync.config_version &&
                  g_sync.last_acked_snapshot;
    pthread_mutex_unlock(&g_sync_lock);
    return allowed;
}
