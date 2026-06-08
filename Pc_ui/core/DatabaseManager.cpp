#include "DatabaseManager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QFileInfo>
#include <QDebug>

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent)
{
    m_flushTimer = new QTimer(this);
    connect(m_flushTimer, &QTimer::timeout, this, &DatabaseManager::flushTelemetryBatch);
    m_flushTimer->start(3000);
}

bool DatabaseManager::openDatabase(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        emit dbError(m_db.lastError().text());
        return false;
    }
    return true;
}

bool DatabaseManager::initTables()
{
    return execSql("CREATE TABLE IF NOT EXISTS telemetry_history ("
                   "id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, factory_id TEXT, area_id TEXT, area_name TEXT, gateway_id TEXT, "
                   "master_slot INTEGER, master_name TEXT, slave_addr INTEGER, device_id INTEGER, device_name TEXT, device_type TEXT, "
                   "temperature REAL, humidity REAL, led INTEGER, fan INTEGER, buzzer INTEGER, voltage REAL, current REAL, power REAL, energy REAL, valid INTEGER)")
        && execSql("CREATE TABLE IF NOT EXISTS alarm_log (id INTEGER PRIMARY KEY AUTOINCREMENT, alarm_id TEXT UNIQUE, factory_id TEXT, area_id TEXT, area_name TEXT, gateway_id TEXT, master_slot INTEGER, slave_addr INTEGER, device_name TEXT, device_type TEXT, alarm_type TEXT, level TEXT, message TEXT, value REAL, threshold REAL, state TEXT, start_time INTEGER, ack_time INTEGER, recover_time INTEGER)")
        && execSql("CREATE TABLE IF NOT EXISTS command_log (id INTEGER PRIMARY KEY AUTOINCREMENT, cmd_id TEXT UNIQUE, timestamp INTEGER, factory_id TEXT, area_id TEXT, gateway_id TEXT, master_slot INTEGER, slave_addr INTEGER, device_type TEXT, command TEXT, params TEXT, state TEXT, ok INTEGER, reason TEXT, ack_time INTEGER)");
}

void DatabaseManager::enqueueTelemetry(const TelemetryRecord &record)
{
    m_telemetryQueue.append(record);
}

void DatabaseManager::enqueueAlarm(const AlarmRecord &record)
{
    if (!m_db.isOpen()) return;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO alarm_log(alarm_id,factory_id,area_id,area_name,gateway_id,master_slot,slave_addr,device_name,device_type,alarm_type,level,message,value,threshold,state,start_time,ack_time,recover_time) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(record.alarmId); q.addBindValue(record.factoryId); q.addBindValue(record.areaId); q.addBindValue(record.areaName); q.addBindValue(record.gatewayId);
    q.addBindValue(record.masterSlot); q.addBindValue(record.slaveAddr); q.addBindValue(record.deviceName); q.addBindValue(record.deviceType);
    q.addBindValue(record.alarmType); q.addBindValue(record.level); q.addBindValue(record.message); q.addBindValue(record.value); q.addBindValue(record.threshold); q.addBindValue(record.state);
    q.addBindValue(record.startTime); q.addBindValue(record.ackTime); q.addBindValue(record.recoverTime);
    q.exec();
}

void DatabaseManager::enqueueCommand(const CommandRecord &record)
{
    if (!m_db.isOpen()) return;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR REPLACE INTO command_log(cmd_id,timestamp,factory_id,area_id,gateway_id,master_slot,slave_addr,device_type,command,params,state,ok,reason,ack_time) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    q.addBindValue(record.cmdId); q.addBindValue(record.timestamp); q.addBindValue(record.factoryId); q.addBindValue(record.areaId); q.addBindValue(record.gatewayId);
    q.addBindValue(record.masterSlot); q.addBindValue(record.slaveAddr); q.addBindValue(record.deviceType); q.addBindValue(record.command); q.addBindValue(record.paramsJson);
    q.addBindValue(record.state); q.addBindValue(record.ok ? 1 : 0); q.addBindValue(record.reason); q.addBindValue(record.ackTime);
    q.exec();
}

void DatabaseManager::flushTelemetryBatch()
{
    if (!m_db.isOpen() || m_telemetryQueue.isEmpty()) return;
    const auto batch = m_telemetryQueue;
    m_telemetryQueue.clear();

    m_db.transaction();
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO telemetry_history(timestamp,factory_id,area_id,area_name,gateway_id,master_slot,master_name,slave_addr,device_id,device_name,device_type,temperature,humidity,led,fan,buzzer,voltage,current,power,energy,valid) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
    for (const auto &r : batch) {
        const auto &d = r.data;
        q.addBindValue(d.timestamp); q.addBindValue(d.node.factoryId); q.addBindValue(d.node.areaId); q.addBindValue(d.node.areaName); q.addBindValue(d.node.gatewayId);
        q.addBindValue(d.node.masterSlot); q.addBindValue(d.node.masterName); q.addBindValue(d.node.slaveAddr); q.addBindValue(d.node.deviceId); q.addBindValue(d.node.deviceName); q.addBindValue(d.node.deviceType);
        q.addBindValue(d.sensorTh.temperature); q.addBindValue(d.sensorTh.humidity);
        q.addBindValue(d.relay.led ? 1 : 0); q.addBindValue(d.relay.fan ? 1 : 0); q.addBindValue(d.relay.buzzer ? 1 : 0);
        q.addBindValue(d.meter.voltage); q.addBindValue(d.meter.current); q.addBindValue(d.meter.power); q.addBindValue(d.meter.energy); q.addBindValue(d.valid ? 1 : 0);
        q.exec();
    }
    m_db.commit();
}

bool DatabaseManager::execSql(const QString &sql)
{
    QSqlQuery q(m_db);
    if (!q.exec(sql)) {
        emit dbError(q.lastError().text());
        return false;
    }
    return true;
}
