#include "pagetrend.h"
#include <QPainter>

PageTrend::PageTrend(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

/* ================= 接收数据 ================= */
void PageTrend::addData(const DataPack &pack)
{
    for (const auto &dev : pack.devices) {
        if (dev.type == DEV_SENSOR_TH) {

            if (!baseInited) {
                // ⭐ 第一次数据作为基准
                tempBase = dev.temperature;
                humiBase = dev.humidity;
                baseInited = true;
            }

            tempList.append(dev.temperature);
            humiList.append(dev.humidity);
        }
    }

    if (tempList.size() > maxPoints) {
        tempList.remove(0);
        humiList.remove(0);
    }

    update();
}

/* ================= 画图 ================= */
void PageTrend::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::black);

    if (tempList.size() < 2 || !baseInited)
        return;

    /* ========= 区域划分 ========= */
    QRect tempRect(0, 60, width(), (height() - 60) / 2);
    QRect humiRect(0, tempRect.bottom(), width(), (height() - 60) / 2);

    /* ========= 中线 ========= */
    int tempMidY = tempRect.center().y();
    int humiMidY = humiRect.center().y();

    /* ========= 画温度曲线 ========= */
    p.setPen(QPen(Qt::white, 2));
    for (int i = 1; i < tempList.size(); ++i) {

        double x1 = (i - 1) * width() / double(maxPoints);
        double x2 = i       * width() / double(maxPoints);

        double y1 = tempMidY - (tempList[i - 1] - tempBase) * tempPixelScale;
        double y2 = tempMidY - (tempList[i]     - tempBase) * tempPixelScale;

        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    /* ========= 画湿度曲线 ========= */
    p.setPen(QPen(Qt::blue, 2));
    for (int i = 1; i < humiList.size(); ++i) {

        double x1 = (i - 1) * width() / double(maxPoints);
        double x2 = i       * width() / double(maxPoints);

        double y1 = humiMidY - (humiList[i - 1] - humiBase) * humiPixelScale;
        double y2 = humiMidY - (humiList[i]     - humiBase) * humiPixelScale;

        p.drawLine(QPointF(x1, y1), QPointF(x2, y2));
    }

    /* ========= 分割线 ========= */
    p.setPen(QPen(Qt::gray, 1));
    p.drawLine(0, tempRect.bottom(), width(), tempRect.bottom());

    /* ========= 左上角文字 ========= */
    p.setPen(Qt::white);
    p.drawText(10, 20, QString("Temp: %1 ℃").arg(tempList.last(), 0, 'f', 1));
    p.drawText(10, 40, QString("Humi: %1 %").arg(humiList.last(), 0, 'f', 1));
}
