#pragma once

#include "OfflineCachePolicy.hpp"

#include <stdint.h>
#include <sqlite3.h>
#include <string>
#include <vector>

struct OfflinePendingMessage {
    int64_t id = 0;
    std::string topic;
    std::string payload;
};

class OfflineCacheDatabase {
public:
    OfflineCacheDatabase() = default;
    ~OfflineCacheDatabase();

    bool open(const std::string &databasePath);
    bool initSchema();
    bool insert(const std::string &topic,
                const std::string &payload,
                const PublishMeta &meta);
    std::vector<OfflinePendingMessage> loadPending(int limit);
    bool remove(int64_t id);
    bool markSendFailed(int64_t id, int64_t nowMs);
    int countPending();
    void enforceMaxRecords();

private:
    bool execSql(const char *sql, const char *logName);
    bool ensureParentDirectory(const std::string &databasePath);
    int64_t countAllRecords();
    int deleteOldByPriority(int priority, int limit);

    sqlite3 *db_ = nullptr;
};
