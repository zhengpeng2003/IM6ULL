#include "alarm_config.h"

#include "data_telemetry.h"
#include "ipc_server.h"
#include "mqtt_wrapper.h"
#include "OfflinePublishQueueC.h"

#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define ALARM_CONFIG_PATH "/etc/qt_object/config.json"
#define ALARM_CONFIG_DIR  "/etc/qt_object"
#define DEFAULT_TEMP_HIGH 35.0f
#define DEFAULT_HUMI_HIGH 80.0f

typedef enum {
    ALARM_REASON_NONE = 0,
    ALARM_REASON_TEMP_HIGH,
    ALARM_REASON_HUMI_HIGH,
    ALARM_REASON_TEMP_HUMI_HIGH
} alarm_reason_t;

static float g_temp_high = DEFAULT_TEMP_HIGH;
static float g_humi_high = DEFAULT_HUMI_HIGH;
static alarm_reason_t g_last_reason = ALARM_REASON_NONE;
static pthread_mutex_t g_alarm_lock = PTHREAD_MUTEX_INITIALIZER;

static const char *alarm_reason_text(alarm_reason_t reason)
{
    switch (reason) {
    case ALARM_REASON_TEMP_HIGH: return "temp_high";
    case ALARM_REASON_HUMI_HIGH: return "humi_high";
    case ALARM_REASON_TEMP_HUMI_HIGH: return "temp_humi_high";
    default: return "";
    }
}

static alarm_reason_t alarm_reason_from_values(float temp, float humi, float temp_high, float humi_high)
{
    int temp_alarm = temp > temp_high;
    int humi_alarm = humi > humi_high;

    if (temp_alarm && humi_alarm)
        return ALARM_REASON_TEMP_HUMI_HIGH;
    if (temp_alarm)
        return ALARM_REASON_TEMP_HIGH;
    if (humi_alarm)
        return ALARM_REASON_HUMI_HIGH;
    return ALARM_REASON_NONE;
}

static int write_config_values(float temp_high, float humi_high)
{
    mkdir(ALARM_CONFIG_DIR, 0755);

    struct json_object *root = json_object_new_object();
    if (!root)
        return -1;

    json_object_object_add(root, "temp_high", json_object_new_double(temp_high));
    json_object_object_add(root, "humi_high", json_object_new_double(humi_high));

    FILE *fp = fopen(ALARM_CONFIG_PATH, "w");
    if (!fp) {
        json_object_put(root);
        return -1;
    }

    const char *text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    int ret = fputs(text, fp) < 0 ? -1 : 0;
    fclose(fp);
    json_object_put(root);
    return ret;
}

void alarm_config_init(void)
{
    pthread_mutex_lock(&g_alarm_lock);

    g_temp_high = DEFAULT_TEMP_HIGH;
    g_humi_high = DEFAULT_HUMI_HIGH;
    g_last_reason = ALARM_REASON_NONE;

    struct json_object *root = json_object_from_file(ALARM_CONFIG_PATH);
    if (root) {
        struct json_object *v;
        if (json_object_object_get_ex(root, "temp_high", &v))
            g_temp_high = (float)json_object_get_double(v);
        if (json_object_object_get_ex(root, "humi_high", &v))
            g_humi_high = (float)json_object_get_double(v);
        json_object_put(root);
    }

    pthread_mutex_unlock(&g_alarm_lock);
}

void alarm_config_get(float *temp_high, float *humi_high)
{
    pthread_mutex_lock(&g_alarm_lock);
    if (temp_high)
        *temp_high = g_temp_high;
    if (humi_high)
        *humi_high = g_humi_high;
    pthread_mutex_unlock(&g_alarm_lock);
}

int alarm_config_set(float temp_high, float humi_high, char *reason, int reason_size)
{
    if (temp_high <= 0.0f || humi_high <= 0.0f) {
        if (reason && reason_size > 0)
            snprintf(reason, reason_size, "invalid_request");
        return -1;
    }

    pthread_mutex_lock(&g_alarm_lock);
    int ret = write_config_values(temp_high, humi_high);
    if (ret == 0) {
        g_temp_high = temp_high;
        g_humi_high = humi_high;
        g_last_reason = ALARM_REASON_NONE;
    }
    pthread_mutex_unlock(&g_alarm_lock);

    if (ret != 0 && reason && reason_size > 0)
        snprintf(reason, reason_size, "config_write_failed");

    return ret;
}

void alarm_config_check_sensor(float temp, float humi)
{
    float temp_high;
    float humi_high;
    alarm_reason_t reason;
    int should_send = 0;

    pthread_mutex_lock(&g_alarm_lock);
    temp_high = g_temp_high;
    humi_high = g_humi_high;
    reason = alarm_reason_from_values(temp, humi, temp_high, humi_high);

    if (reason == ALARM_REASON_NONE) {
        g_last_reason = ALARM_REASON_NONE;
    } else if (reason != g_last_reason) {
        g_last_reason = reason;
        should_send = 1;
    }
    pthread_mutex_unlock(&g_alarm_lock);

    if (!should_send)
        return;

    struct json_object *root = json_object_new_object();
    if (!root)
        return;

    json_object_object_add(root, "type", json_object_new_string("command"));
    json_object_object_add(root, "cmd", json_object_new_string("emergency"));
    json_object_object_add(root, "level", json_object_new_int(1));
    json_object_object_add(root, "reason", json_object_new_string(alarm_reason_text(reason)));
    json_object_object_add(root, "temp", json_object_new_double(temp));
    json_object_object_add(root, "humi", json_object_new_double(humi));
    json_object_object_add(root, "temp_high", json_object_new_double(temp_high));
    json_object_object_add(root, "humi_high", json_object_new_double(humi_high));

    const char *payload = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    ipc_server_send(payload);
    offline_publish_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.message_type = "alarm_event";
    meta.gateway_id = DEFAULT_GATEWAY_ID;
    meta.priority = 3;
    meta.has_alarm = 1;
    (void)offline_publish_or_cache(MQTT_DEFAULT_PUBLISH_TOPIC, payload, &meta);
    json_object_put(root);
}
