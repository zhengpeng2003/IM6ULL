#include "pagetrend.h"

#include <QPainter>

#include "ipc/ipc_client.h"
#include "ui/widget.h"

PageTrend::PageTrend(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(Widget::_Myclient, &IpcClient::portStatusUpdated,
            this, &PageTrend::onPortStatusUpdated);
}

void PageTrend::addData(const DataPack &pack)
{
    if (!sensorConnected)
        return;

    for (const auto &dev : pack.devices) {
        if (dev.type != DEV_SENSOR_TH || !dev.valid)
            continue;

        if (!baseInited) {
            tempBase = dev.temperature;
            humiBase = dev.humidity;
            baseInited = true;
        }

        tempList.append(dev.temperature);
        humiList.append(dev.humidity);
    }

    while (tempList.size() > maxPoints) {
        tempList.remove(0);
        humiList.remove(0);
    }

    update();
}

void PageTrend::onPortStatusUpdated(int slot,
                                    const QString &port,
                                    const QString &deviceType,
                                    int baud,
                                    bool connected,
                                    const QString &message)
{
    Q_UNUSED(slot)
    Q_UNUSED(port)
    Q_UNUSED(baud)
    Q_UNUSED(message)

    if (deviceType != "sensor_th")
        return;

    sensorConnected = connected;
    if (!connected) {
        tempList.clear();
        humiList.clear();
        baseInited = false;
    }
    update();
}

void PageTrend::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#1E1E1E"));

    if (!sensorConnected) {
        p.setPen(QColor("#AEB4B8"));
        p.drawText(rect(), Qt::AlignCenter, "未连接温湿度设备\n连接后显示实时曲线");
        return;
    }

    if (tempList.size() < 2 || !baseInited) {
        p.setPen(QColor("#AEB4B8"));
        p.drawText(rect(), Qt::AlignCenter, "等待温湿度数据...");
        return;
    }

    QRect tempRect(0, 60, width(), (height() - 60) / 2);
    QRect humiRect(0, tempRect.bottom(), width(), (height() - 60) / 2);

    int tempMidY = tempRect.center().y();
    int humiMidY = humiRect.center().y();

    p.setPen(QPen(QColor("#F0F0F0"), 2));
    for (int i = 1; i < tempList.size(); ++i) {
        double x1 = (i - 1) * width() / double(maxPoints);
        double x2 = i * width() / double(maxPoints);
        double y1 = tempMidY - (tempList[i - 1] - tempBase) * tempPixelScale;
        double y2 = tempMidY - (tempList[i] - tempBase) * tempPixelScale;
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    p.setPen(QPen(QColor("#4DA3FF"), 2));
    for (int i = 1; i < humiList.size(); ++i) {
        double x1 = (i - 1) * width() / double(maxPoints);
        double x2 = i * width() / double(maxPoints);
        double y1 = humiMidY - (humiList[i - 1] - humiBase) * humiPixelScale;
        double y2 = humiMidY - (humiList[i] - humiBase) * humiPixelScale;
        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    p.setPen(QPen(QColor("#565656"), 1));
    p.drawLine(0, tempRect.bottom(), width(), tempRect.bottom());

    p.setPen(QColor("#E0E0E0"));
    p.drawText(10, 20, QString("Temp: %1 C").arg(tempList.last(), 0, 'f', 1));
    p.drawText(10, 40, QString("Humi: %1 %").arg(humiList.last(), 0, 'f', 1));
}
