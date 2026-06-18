#include "TopStatusBar.h"

#include <QStyle>
#include <QTime>

TopStatusBar::TopStatusBar(QWidget *parent)
    : QWidget(parent)
{
    initUI();
    initSignal();
}

void TopStatusBar::setBackendConnected(bool connected)
{
    titleLabel->setText(connected
        ? "工业物联网终端"
        : "工业物联网终端 - IPC未连接");
    statusDot->setObjectName(connected ? "StatusGreen" : "StatusGray");
    statusDot->style()->unpolish(statusDot);
    statusDot->style()->polish(statusDot);
}

QString TopStatusBar::textForConfigSyncState(const QString &status,
                                             const QString &reason,
                                             const QString &message,
                                             int retryCount) const
{
    if (status == "offline")
        return "MQTT 未连接，注册表仅本地保存";
    if (status == "syncing")
        return "注册表同步中，等待 Pc_data 确认";
    if (status == "timeout")
        return "未收到 Pc_data 确认，正在重试";
    if (status == "retrying")
        return "Pc_data 长时间无响应，请检查连接";
    if (status == "failed") {
        const QString detail = !message.isEmpty() ? message : reason;
        return detail.isEmpty() ? "注册表同步失败" : QString("注册表同步失败：%1").arg(detail);
    }
    if (status == "partial_failed") {
        const QString detail = !message.isEmpty() ? message : reason;
        return detail.isEmpty() ? "部分设备注册失败" : QString("部分设备注册失败：%1").arg(detail);
    }
    if (status == "success")
        return "注册表已同步";
    if (retryCount > 0)
        return "未收到 Pc_data 确认，正在重试";
    return "工业物联网终端";
}

void TopStatusBar::setConfigSyncState(const QString &status,
                                      const QString &reason,
                                      const QString &message,
                                      int retryCount)
{
    titleLabel->setText(textForConfigSyncState(status, reason, message, retryCount));

    QString dotName = "StatusGreen";
    if (status == "success")
        dotName = "StatusGreen";
    else if (status == "failed" || status == "partial_failed")
        dotName = "StatusRed";
    else if (status == "offline")
        dotName = "StatusGray";
    else if (status == "syncing" || status == "timeout" || status == "retrying")
        dotName = "StatusYellow";

    statusDot->setObjectName(dotName);
    statusDot->style()->unpolish(statusDot);
    statusDot->style()->polish(statusDot);
}

void TopStatusBar::initUI()
{
    setFixedHeight(28);
    setObjectName("TopStatusBar");

    titleLabel = new QLabel("工业物联网终端");
    timeLabel = new QLabel("00:00");
    statusDot = new QLabel;

    statusDot->setFixedSize(10, 10);
    statusDot->setObjectName("StatusGreen");

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 0, 8, 0);
    layout->setSpacing(6);

    layout->addWidget(titleLabel);
    layout->addStretch();
    layout->addWidget(timeLabel);
    layout->addWidget(statusDot);

    setBackendConnected(true);
}

void TopStatusBar::initSignal()
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        timeLabel->setText(QTime::currentTime().toString("HH:mm"));
    });
    timer->start(1000);
}
