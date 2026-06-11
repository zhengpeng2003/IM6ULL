#pragma once
#include <QJsonObject>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;

class SystemSettingPage : public QWidget
{
    Q_OBJECT
public:
    explicit SystemSettingPage(QWidget *parent = nullptr);

    void setIpcConnected(bool connected);
    void onMqttConfigMessage(const QJsonObject &obj);
    void onMqttConfigAck(const QJsonObject &obj);

signals:
    void mqttConfigRequested();
    void mqttConfigSaveRequested(const QString &host, int port);

private:
    QWidget *createCard(const QString &title, QWidget *content);
    QWidget *createMqttCard();
    QWidget *createDatabaseCard();
    QWidget *createCollectCard();
    QWidget *createAboutCard();
    QLabel *createFieldLabel(const QString &text);
    void setStatusText(const QString &text, bool ok);
    bool validateInput(QString &host, int &port) const;

private:
    QLineEdit *m_hostEdit = nullptr;
    QLineEdit *m_portEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_testButton = nullptr;
    QPushButton *m_saveButton = nullptr;
    bool m_ipcConnected = false;
};
