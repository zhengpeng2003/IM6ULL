#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QTimer>
#include <QList>
#include "model/TelemetryModel.h"
#include "model/AlarmModel.h"
#include "model/CommandModel.h"

class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    explicit DatabaseManager(QObject *parent = nullptr);
    bool openDatabase(const QString &path);
    bool initTables();

public slots:
    void enqueueTelemetry(const TelemetryRecord &record);
    void enqueueAlarm(const AlarmRecord &record);
    void enqueueCommand(const CommandRecord &record);
    void flushTelemetryBatch();

signals:
    void dbError(const QString &error);

private:
    bool execSql(const QString &sql);

private:
    QSqlDatabase m_db;
    QList<TelemetryRecord> m_telemetryQueue;
    QTimer *m_flushTimer = nullptr;
};
