#include "SensorThDetailCardUi.h"

#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QString displayPointValue(const TelemetryPointData &point)
{
    if (!point.valid) {
        return point.errorMessage.isEmpty()
            ? QStringLiteral("无效")
            : QStringLiteral("无效(%1)").arg(point.errorMessage);
    }

    if (point.valueType == QStringLiteral("text")) {
        return point.textValue;
    }

    if (point.valueType == QStringLiteral("boolean")) {
        return point.numberValue != 0.0 ? QStringLiteral("ON") : QStringLiteral("OFF");
    }

    QString text = QString::number(point.numberValue, 'f', 2);
    if (!point.unit.isEmpty()) {
        text += QStringLiteral(" ") + point.unit;
    }
    return text;
}

QString displayPointName(const TelemetryPointData &point)
{
    return point.pointName.isEmpty() ? point.pointKey : point.pointName;
}

QWidget *createMetricCard(const QString &titleText, QLabel **valueLabel, QWidget *parent)
{
    auto *card = new QWidget(parent);
    card->setObjectName("MetricCard");

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 14);
    layout->setSpacing(8);

    auto *title = new QLabel(titleText, card);
    title->setObjectName("MetricTitle");
    layout->addWidget(title);

    *valueLabel = new QLabel(QStringLiteral("-"), card);
    (*valueLabel)->setObjectName("MetricValue");
    layout->addWidget(*valueLabel);

    return card;
}

} // namespace

SensorThDetailCardUi::SensorThDetailCardUi(QWidget *parent)
    : DeviceDetailCardBaseUi(parent)
{
    setObjectName("SensorThDetailCardUi");

    auto *metrics = new QGridLayout;
    metrics->setContentsMargins(0, 0, 0, 0);
    metrics->setHorizontalSpacing(12);
    metrics->setVerticalSpacing(12);

    m_temperatureCard = createMetricCard(QStringLiteral("温度"), &m_temperatureValue, this);
    m_humidityCard = createMetricCard(QStringLiteral("湿度"), &m_humidityValue, this);
    metrics->addWidget(m_temperatureCard, 0, 0);
    metrics->addWidget(m_humidityCard, 0, 1);
    contentLayout()->addLayout(metrics);

    m_extraCard = new QWidget(this);
    m_extraCard->setObjectName("MetricCard");
    auto *extraRoot = new QVBoxLayout(m_extraCard);
    extraRoot->setContentsMargins(14, 12, 14, 14);
    extraRoot->setSpacing(8);

    auto *extraTitle = new QLabel(QStringLiteral("其他测点"), m_extraCard);
    extraTitle->setObjectName("MetricTitle");
    extraRoot->addWidget(extraTitle);

    m_extraLayout = new QVBoxLayout;
    m_extraLayout->setContentsMargins(0, 0, 0, 0);
    m_extraLayout->setSpacing(6);
    extraRoot->addLayout(m_extraLayout);
    contentLayout()->addWidget(m_extraCard);
}

void SensorThDetailCardUi::setDeviceData(const RealtimeDeviceData &data)
{
    DeviceDetailCardBaseUi::setDeviceData(data);

    const TelemetryPointData *temperature = nullptr;
    const TelemetryPointData *humidity = nullptr;
    QList<TelemetryPointData> extraPoints;

    for (const TelemetryPointData &point : data.points) {
        if (point.pointKey == QStringLiteral("temperature")) {
            temperature = &point;
        } else if (point.pointKey == QStringLiteral("humidity")) {
            humidity = &point;
        } else {
            extraPoints.append(point);
        }
    }

    setMetric(m_temperatureCard,
              m_temperatureValue,
              temperature ? displayPointValue(*temperature)
                          : QStringLiteral("%1 ℃").arg(data.sensorTh.temperature, 0, 'f', 2));
    setMetric(m_humidityCard,
              m_humidityValue,
              humidity ? displayPointValue(*humidity)
                       : QStringLiteral("%1 %RH").arg(data.sensorTh.humidity, 0, 'f', 2));

    std::sort(extraPoints.begin(), extraPoints.end(), [](const TelemetryPointData &left, const TelemetryPointData &right) {
        return left.pointKey < right.pointKey;
    });

    clearExtraRows();
    if (extraPoints.isEmpty()) {
        auto *empty = new QLabel(QStringLiteral("暂无"), m_extraCard);
        empty->setObjectName("MetricValue");
        m_extraLayout->addWidget(empty);
    } else {
        for (const TelemetryPointData &point : extraPoints) {
            auto *row = new QLabel(QStringLiteral("%1：%2").arg(displayPointName(point), displayPointValue(point)), m_extraCard);
            row->setObjectName("MetricValue");
            row->setWordWrap(true);
            m_extraLayout->addWidget(row);
        }
    }
}

void SensorThDetailCardUi::setMetric(QWidget *card, QLabel *valueLabel, const QString &value)
{
    if (card) {
        card->show();
    }
    if (valueLabel) {
        valueLabel->setText(value);
    }
}

void SensorThDetailCardUi::clearExtraRows()
{
    while (QLayoutItem *item = m_extraLayout->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}
