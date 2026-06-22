#pragma once

#include <QHash>
#include <QWidget>

#include "model/TelemetryModel.h"

class QGridLayout;
class QLabel;
class QVBoxLayout;

class DeviceDetailCardBaseUi : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceDetailCardBaseUi(QWidget *parent = nullptr);

    virtual void setDeviceData(const RealtimeDeviceData &data);
    void setMessage(const QString &message);

protected:
    QVBoxLayout *contentLayout() const;

private:
    void addInfoRow(int row, const QString &key, const QString &title);
    void setInfoValue(const QString &key, const QString &value);
    void clearInfoValues();

private:
    QLabel *m_messageLabel = nullptr;
    QWidget *m_infoCard = nullptr;
    QGridLayout *m_infoLayout = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QHash<QString, QLabel *> m_valueLabels;
};
