#include "OfflineCacheDatabase.hpp"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

namespace {

const int kMaxOfflineRecords = 5000;
const int kCleanupBatchSize = 100;

int64_t currentTimeMs()
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
        return (int64_t)time(NULL) * 1000;

    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void bindText(sqlite3_stmt *stmt, int index, const std::string &value)
{
    sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
}

} // namespace

OfflineCacheDatabase::~OfflineCacheDatabase()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool OfflineCacheDatabase::open(const std::string &databasePath)
{
    ensureParentDirectory(databasePath);

    if (sqlite3_open(databasePath.c_str(), &db_) != SQLITE_OK) {
        printf("[OfflineCache] open database failed path=%s err=%s\n",
               databasePath.c_str(),
               db_ ? sqlite3_errmsg(db_) : "unknown");
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        return false;
    }

    sqlite3_busy_timeout(db_, 1000);
    printf("[OfflineCache] database opened path=%s\n", databasePath.c_str());
    return true;
}

bool OfflineCacheDatabase::initSchema()
{
    const char *create_table =
        "CREATE TABLE IF NOT EXISTS offline_publish_queue ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "topic TEXT NOT NULL,"
        "payload TEXT NOT NULL,"
        "message_type TEXT NOT NULL,"
        "priority INTEGER NOT NULL,"
        "gateway_id TEXT,"
        "port_id TEXT,"
        "device_id INTEGER,"
        "point_key TEXT,"
        "timestamp_ms INTEGER NOT NULL,"
        "create_time_ms INTEGER NOT NULL,"
        "send_count INTEGER DEFAULT 0,"
        "last_send_time_ms INTEGER DEFAULT 0,"
        "status TEXT DEFAULT 'pending'"
        ");";

    const char *idx_status =
        "CREATE INDEX IF NOT EXISTS idx_offline_queue_status_priority_time "
        "ON offline_publish_queue(status, priority, timestamp_ms);";

    const char *idx_device =
        "CREATE INDEX IF NOT EXISTS idx_offline_queue_device_point "
        "ON offline_publish_queue(gateway_id, port_id, device_id, point_key);";

    if (!execSql(create_table, "create offline_publish_queue"))
        return false;
    if (!execSql(idx_status, "create status index"))
        return false;
    if (!execSql(idx_device, "create device index"))
        return false;

    return true;
}

bool OfflineCacheDatabase::insert(const std::string &topic,
                                  const std::string &payload,
                                  const PublishMeta &meta)
{
    if (!db_)
        return false;

    const char *sql =
        "INSERT INTO offline_publish_queue ("
        "topic, payload, message_type, priority, gateway_id, port_id, "
        "device_id, point_key, timestamp_ms, create_time_ms, status"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'pending');";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        printf("[OfflineCache] prepare insert failed err=%s\n", sqlite3_errmsg(db_));
        return false;
    }

    const int64_t timestamp_ms = meta.timestampMs > 0 ? meta.timestampMs : currentTimeMs();
    bindText(stmt, 1, topic);
    bindText(stmt, 2, payload);
    bindText(stmt, 3, meta.messageType);
    sqlite3_bind_int(stmt, 4, meta.priority);
    bindText(stmt, 5, meta.gatewayId);
    bindText(stmt, 6, meta.portId);
    sqlite3_bind_int(stmt, 7, meta.deviceId);
    bindText(stmt, 8, meta.pointKey);
    sqlite3_bind_int64(stmt, 9, timestamp_ms);
    sqlite3_bind_int64(stmt, 10, currentTimeMs());

    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        printf("[OfflineCache] insert failed type=%s priority=%d err=%s\n",
               meta.messageType.c_str(), meta.priority, sqlite3_errmsg(db_));
        return false;
    }

    printf("[OfflineCache] cached message type=%s priority=%d topic=%s\n",
           meta.messageType.c_str(), meta.priority, topic.c_str());
    return true;
}

std::vector<OfflinePendingMessage> OfflineCacheDatabase::loadPending(int limit)
{
    std::vector<OfflinePendingMessage> rows;
    if (!db_ || limit <= 0)
        return rows;

    const char *sql =
        "SELECT id, topic, payload "
        "FROM offline_publish_queue "
        "WHERE status = 'pending' "
        "ORDER BY priority DESC, timestamp_ms ASC "
        "LIMIT ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        printf("[OfflineCache] prepare load pending failed err=%s\n", sqlite3_errmsg(db_));
        return rows;
    }

    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        OfflinePendingMessage row;
        row.id = sqlite3_column_int64(stmt, 0);
        const unsigned char *topic = sqlite3_column_text(stmt, 1);
        const unsigned char *payload = sqlite3_column_text(stmt, 2);
        row.topic = topic ? reinterpret_cast<const char *>(topic) : "";
        row.payload = payload ? reinterpret_cast<const char *>(payload) : "";
        rows.push_back(row);
    }

    sqlite3_finalize(stmt);
    return rows;
}

bool OfflineCacheDatabase::remove(int64_t id)
{
    if (!db_)
        return false;

    sqlite3_stmt *stmt = nullptr;
    const char *sql = "DELETE FROM offline_publish_queue WHERE id = ?;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, id);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        printf("[OfflineCache] resend success, removed id=%lld\n", (long long)id);
        return true;
    }

    printf("[OfflineCache] remove failed id=%lld err=%s\n",
           (long long)id, sqlite3_errmsg(db_));
    return false;
}

bool OfflineCacheDatabase::markSendFailed(int64_t id, int64_t nowMs)
{
    if (!db_)
        return false;

    const char *sql =
        "UPDATE offline_publish_queue "
        "SET send_count = send_count + 1, "
        "last_send_time_ms = ?, "
        "status = CASE WHEN send_count + 1 >= 5 THEN 'failed' ELSE status END "
        "WHERE id = ?;";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int64(stmt, 1, nowMs);
    sqlite3_bind_int64(stmt, 2, id);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        printf("[OfflineCache] resend failed, send_count increased id=%lld\n", (long long)id);
        return true;
    }

    printf("[OfflineCache] mark send failed failed id=%lld err=%s\n",
           (long long)id, sqlite3_errmsg(db_));
    return false;
}

int OfflineCacheDatabase::countPending()
{
    if (!db_)
        return 0;

    sqlite3_stmt *stmt = nullptr;
    const char *sql = "SELECT COUNT(*) FROM offline_publish_queue WHERE status = 'pending';";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return count;
}

bool OfflineCacheDatabase::clearPending()
{
    if (!db_)
        return false;

    const bool ok = execSql("DELETE FROM offline_publish_queue;", "clear offline cache");
    printf("[OfflineCache] clear cache %s\n", ok ? "ok" : "failed");
    return ok;
}

void OfflineCacheDatabase::enforceMaxRecords()
{
    int64_t total = countAllRecords();
    if (total <= kMaxOfflineRecords)
        return;

    printf("[OfflineCache] cache exceeds max records total=%lld max=%d\n",
           (long long)total, kMaxOfflineRecords);

    for (int priority = 0; priority <= 1 && total > kMaxOfflineRecords; ++priority) {
        while (total > kMaxOfflineRecords) {
            int deleted = deleteOldByPriority(priority, kCleanupBatchSize);
            if (deleted <= 0)
                break;
            total -= deleted;
            printf("[OfflineCache] removed old low priority records priority=%d count=%d total=%lld\n",
                   priority, deleted, (long long)total);
        }
    }

    if (total > kMaxOfflineRecords) {
        printf("[OfflineCache] cache still exceeds max after low priority cleanup total=%lld\n",
               (long long)total);
    }
}

bool OfflineCacheDatabase::execSql(const char *sql, const char *logName)
{
    if (!db_)
        return false;

    char *err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        printf("[OfflineCache] %s failed err=%s\n", logName ? logName : "exec", err ? err : "");
        sqlite3_free(err);
        return false;
    }

    return true;
}

bool OfflineCacheDatabase::ensureParentDirectory(const std::string &databasePath)
{
    const std::string::size_type pos = databasePath.find_last_of('/');
    if (pos == std::string::npos || pos == 0)
        return true;

    const std::string dir = databasePath.substr(0, pos);
    if (mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST) {
        printf("[OfflineCache] create database directory failed dir=%s\n", dir.c_str());
        return false;
    }

    return true;
}

int64_t OfflineCacheDatabase::countAllRecords()
{
    if (!db_)
        return 0;

    sqlite3_stmt *stmt = nullptr;
    const char *sql = "SELECT COUNT(*) FROM offline_publish_queue;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int64_t count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        count = sqlite3_column_int64(stmt, 0);

    sqlite3_finalize(stmt);
    return count;
}

int OfflineCacheDatabase::deleteOldByPriority(int priority, int limit)
{
    if (!db_)
        return 0;

    const char *sql =
        "DELETE FROM offline_publish_queue "
        "WHERE id IN ("
        "SELECT id FROM offline_publish_queue "
        "WHERE priority = ? "
        "ORDER BY timestamp_ms ASC "
        "LIMIT ?"
        ");";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    sqlite3_bind_int(stmt, 1, priority);
    sqlite3_bind_int(stmt, 2, limit);
    const int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE)
        return 0;

    return sqlite3_changes(db_);
}
