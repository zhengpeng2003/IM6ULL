#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

class QWidget;

struct DeviceTypeOption
{
    QString type;
    QString displayName;
};

class DeviceTypeConfigRegistry
{
public:
    static QList<DeviceTypeOption> deviceTypes();
    static QWidget *createOptionsWidget(const QString &deviceType, QWidget *parent);
    static bool validateOptions(const QString &deviceType, QWidget *optionsWidget, QWidget *messageParent);
    static QVariantMap collectOptions(const QString &deviceType, QWidget *optionsWidget);
};
