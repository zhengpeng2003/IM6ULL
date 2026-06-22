#include "DeviceTypeConfigRegistry.h"

#include <QComboBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QWidget>

namespace {

constexpr int kRelayCustomValue = -1;
constexpr int kRelayDefaultChannelCount = 8;
constexpr int kRelayMinChannelCount = 1;
constexpr int kRelayMaxChannelCount = 64;

const char *kRelayChannelComboName = "RelayChannelCountCombo";
const char *kRelayCustomSpinName = "RelayCustomChannelCountSpin";

} // namespace

QList<DeviceTypeOption> DeviceTypeConfigRegistry::deviceTypes()
{
    return {
        {QStringLiteral("sensor_th"), QStringLiteral("温湿度传感器")},
        {QStringLiteral("relay"), QStringLiteral("继电器")}
    };
}

QWidget *DeviceTypeConfigRegistry::createOptionsWidget(const QString &deviceType, QWidget *parent)
{
    if (deviceType != QStringLiteral("relay")) {
        return nullptr;
    }

    auto *widget = new QWidget(parent);
    auto *layout = new QFormLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *channelCombo = new QComboBox(widget);
    channelCombo->setObjectName(QString::fromLatin1(kRelayChannelComboName));
    channelCombo->addItem(QStringLiteral("1 路"), 1);
    channelCombo->addItem(QStringLiteral("2 路"), 2);
    channelCombo->addItem(QStringLiteral("4 路"), 4);
    channelCombo->addItem(QStringLiteral("8 路"), 8);
    channelCombo->addItem(QStringLiteral("16 路"), 16);
    channelCombo->addItem(QStringLiteral("自定义"), kRelayCustomValue);
    channelCombo->setCurrentIndex(channelCombo->findData(kRelayDefaultChannelCount));

    auto *customSpin = new QSpinBox(widget);
    customSpin->setObjectName(QString::fromLatin1(kRelayCustomSpinName));
    customSpin->setRange(kRelayMinChannelCount, kRelayMaxChannelCount);
    customSpin->setValue(kRelayDefaultChannelCount);
    customSpin->setSuffix(QStringLiteral(" 路"));
    customSpin->setVisible(false);

    layout->addRow(QStringLiteral("继电器路数"), channelCombo);
    layout->addRow(QStringLiteral("自定义路数"), customSpin);

    QObject::connect(channelCombo,
                     QOverload<int>::of(&QComboBox::currentIndexChanged),
                     widget,
                     [channelCombo, customSpin](int) {
        customSpin->setVisible(channelCombo->currentData().toInt() == kRelayCustomValue);
    });

    return widget;
}

QVariantMap DeviceTypeConfigRegistry::collectOptions(const QString &deviceType, QWidget *optionsWidget)
{
    QVariantMap result;
    if (deviceType != QStringLiteral("relay") || !optionsWidget) {
        return result;
    }

    auto *channelCombo = optionsWidget->findChild<QComboBox *>(QString::fromLatin1(kRelayChannelComboName));
    if (!channelCombo) {
        return result;
    }

    int channelCount = channelCombo->currentData().toInt();
    if (channelCount == kRelayCustomValue) {
        auto *customSpin = optionsWidget->findChild<QSpinBox *>(QString::fromLatin1(kRelayCustomSpinName));
        channelCount = customSpin ? customSpin->value() : kRelayDefaultChannelCount;
    }

    if (channelCount < kRelayMinChannelCount || channelCount > kRelayMaxChannelCount) {
        channelCount = kRelayDefaultChannelCount;
    }

    result.insert(QStringLiteral("relay_channel_count"), channelCount);
    return result;
}
