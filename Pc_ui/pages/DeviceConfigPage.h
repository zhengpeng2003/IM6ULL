#pragma once
#include <QWidget>
class DeviceManager;
class ConfigManager;
class DeviceTreeWidget;
class QLabel;

class DeviceConfigPage : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceConfigPage(DeviceManager *device, ConfigManager *config, QWidget *parent = nullptr);

private:
    DeviceManager *m_device = nullptr;
    ConfigManager *m_config = nullptr;
    DeviceTreeWidget *m_tree = nullptr;
    QLabel *m_detail = nullptr;
};
