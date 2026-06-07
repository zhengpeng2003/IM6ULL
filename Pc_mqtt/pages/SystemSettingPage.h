#pragma once
#include <QWidget>
class ConfigManager;
class MqttClientManager;
class QLineEdit;
class QCheckBox;

class SystemSettingPage : public QWidget
{
    Q_OBJECT
public:
    explicit SystemSettingPage(ConfigManager *config, MqttClientManager *mqtt, QWidget *parent = nullptr);

private slots:
    void saveConfig();
    void testConnect();

private:
    ConfigManager *m_config = nullptr;
    MqttClientManager *m_mqtt = nullptr;
    QLineEdit *m_hostEdit = nullptr;
    QLineEdit *m_portEdit = nullptr;
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QLineEdit *m_clientIdEdit = nullptr;
    QCheckBox *m_autoConnect = nullptr;
    QCheckBox *m_autoReconnect = nullptr;
};
