#pragma once
#include <QObject>
#include <QHash>
#include <QJsonObject>
#include "model/DeviceModel.h"
#include "model/CommandModel.h"

class QTimer;

class CommandManager : public QObject
{
    Q_OBJECT
public:
    explicit CommandManager(QObject *parent = nullptr);

public slots:
    void sendRelayCommand(const DeviceNode &device, const QString &channel, bool value);
    void onCommandAck(const QJsonObject &obj);

signals:
    void commandReadyToPublish(const QString &topic, const QByteArray &payload, int qos, bool retain);
    void commandStateChanged(const QString &cmdId, const QString &state);
    void commandTimeout(const QString &cmdId);
    void commandForDb(const CommandRecord &record);

private:
    QString createCmdId() const;
    QString commandTopic(const DeviceNode &device) const;

private:
    QHash<QString, CommandRecord> m_pending;
};
