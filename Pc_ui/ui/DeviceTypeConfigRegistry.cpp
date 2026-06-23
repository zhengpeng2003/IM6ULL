#include "DeviceTypeConfigRegistry.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr int kRelayCustomValue = -1;
constexpr int kRelayDefaultChannelCount = 8;
constexpr int kRelayMinChannelCount = 1;
constexpr int kRelayMaxChannelCount = 64;

const char *kRelayChannelComboName = "RelayChannelCountCombo";
const char *kRelayCustomSpinName = "RelayCustomChannelCountSpin";

const char *kThresholdEnableName = "SensorThThresholdEnable";
const char *kTempEnableName = "SensorThTempEnable";
const char *kTempLowName = "SensorThTempLow";
const char *kTempHighName = "SensorThTempHigh";
const char *kTempLowSpinName = "SensorThTempLowSpin";
const char *kTempHighSpinName = "SensorThTempHighSpin";
const char *kHumiEnableName = "SensorThHumiEnable";
const char *kHumiLowName = "SensorThHumiLow";
const char *kHumiHighName = "SensorThHumiHigh";
const char *kHumiLowSpinName = "SensorThHumiLowSpin";
const char *kHumiHighSpinName = "SensorThHumiHighSpin";

QCheckBox *findCheckBox(QWidget *widget, const char *name)
{
    return widget ? widget->findChild<QCheckBox *>(QString::fromLatin1(name)) : nullptr;
}

QDoubleSpinBox *findDoubleSpinBox(QWidget *widget, const char *name)
{
    return widget ? widget->findChild<QDoubleSpinBox *>(QString::fromLatin1(name)) : nullptr;
}

QDoubleSpinBox *createThresholdSpin(QWidget *parent, double value, const QString &suffix)
{
    auto *spin = new QDoubleSpinBox(parent);
    spin->setRange(-1000.0, 1000.0);
    spin->setDecimals(1);
    spin->setSingleStep(1.0);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setMinimumWidth(82);
    return spin;
}

QVariantMap pointThresholdPayload(const QCheckBox *enableBox,
                                  const QCheckBox *lowBox,
                                  const QDoubleSpinBox *lowSpin,
                                  const QCheckBox *highBox,
                                  const QDoubleSpinBox *highSpin)
{
    QVariantMap point;
    const bool enabled = enableBox && enableBox->isChecked();
    point.insert(QStringLiteral("enable_alarm"), enabled);
    if (enabled && lowBox && lowBox->isChecked() && lowSpin) {
        point.insert(QStringLiteral("alarm_low"), lowSpin->value());
    }
    if (enabled && highBox && highBox->isChecked() && highSpin) {
        point.insert(QStringLiteral("alarm_high"), highSpin->value());
    }
    return point;
}

QWidget *createSensorThOptionsWidget(QWidget *parent)
{
    auto *widget = new QWidget(parent);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *thresholdEnable = new QCheckBox(QStringLiteral("启用阈值告警"), widget);
    thresholdEnable->setObjectName(QString::fromLatin1(kThresholdEnableName));

    auto *tempEnable = new QCheckBox(QStringLiteral("温度"), widget);
    tempEnable->setObjectName(QString::fromLatin1(kTempEnableName));
    auto *tempLow = new QCheckBox(QStringLiteral("低于"), widget);
    tempLow->setObjectName(QString::fromLatin1(kTempLowName));
    auto *tempHigh = new QCheckBox(QStringLiteral("高于"), widget);
    tempHigh->setObjectName(QString::fromLatin1(kTempHighName));
    auto *tempLowSpin = createThresholdSpin(widget, 10.0, QStringLiteral(" ℃"));
    tempLowSpin->setObjectName(QString::fromLatin1(kTempLowSpinName));
    auto *tempHighSpin = createThresholdSpin(widget, 35.0, QStringLiteral(" ℃"));
    tempHighSpin->setObjectName(QString::fromLatin1(kTempHighSpinName));

    auto *humiEnable = new QCheckBox(QStringLiteral("湿度"), widget);
    humiEnable->setObjectName(QString::fromLatin1(kHumiEnableName));
    auto *humiLow = new QCheckBox(QStringLiteral("低于"), widget);
    humiLow->setObjectName(QString::fromLatin1(kHumiLowName));
    auto *humiHigh = new QCheckBox(QStringLiteral("高于"), widget);
    humiHigh->setObjectName(QString::fromLatin1(kHumiHighName));
    auto *humiLowSpin = createThresholdSpin(widget, 20.0, QStringLiteral(" %"));
    humiLowSpin->setObjectName(QString::fromLatin1(kHumiLowSpinName));
    auto *humiHighSpin = createThresholdSpin(widget, 80.0, QStringLiteral(" %"));
    humiHighSpin->setObjectName(QString::fromLatin1(kHumiHighSpinName));

    auto *tempLayout = new QHBoxLayout;
    tempLayout->setContentsMargins(0, 0, 0, 0);
    tempLayout->addWidget(tempEnable);
    tempLayout->addStretch();
    tempLayout->addWidget(tempLow);
    tempLayout->addWidget(tempLowSpin);
    tempLayout->addWidget(tempHigh);
    tempLayout->addWidget(tempHighSpin);

    auto *humiLayout = new QHBoxLayout;
    humiLayout->setContentsMargins(0, 0, 0, 0);
    humiLayout->addWidget(humiEnable);
    humiLayout->addStretch();
    humiLayout->addWidget(humiLow);
    humiLayout->addWidget(humiLowSpin);
    humiLayout->addWidget(humiHigh);
    humiLayout->addWidget(humiHighSpin);

    layout->addWidget(thresholdEnable);
    layout->addLayout(tempLayout);
    layout->addLayout(humiLayout);

    auto updateControls = [=]() {
        const bool enabled = thresholdEnable->isChecked();
        const bool tempEnabled = enabled && tempEnable->isChecked();
        const bool humiEnabled = enabled && humiEnable->isChecked();

        tempEnable->setEnabled(enabled);
        humiEnable->setEnabled(enabled);
        tempLow->setEnabled(tempEnabled);
        tempHigh->setEnabled(tempEnabled);
        tempLowSpin->setEnabled(tempEnabled && tempLow->isChecked());
        tempHighSpin->setEnabled(tempEnabled && tempHigh->isChecked());
        humiLow->setEnabled(humiEnabled);
        humiHigh->setEnabled(humiEnabled);
        humiLowSpin->setEnabled(humiEnabled && humiLow->isChecked());
        humiHighSpin->setEnabled(humiEnabled && humiHigh->isChecked());
    };
    QObject::connect(thresholdEnable, &QCheckBox::toggled, widget, [=](bool) { updateControls(); });
    QObject::connect(tempEnable, &QCheckBox::toggled, widget, [=](bool) { updateControls(); });
    QObject::connect(tempLow, &QCheckBox::toggled, widget, [=](bool) { updateControls(); });
    QObject::connect(tempHigh, &QCheckBox::toggled, widget, [=](bool) { updateControls(); });
    QObject::connect(humiEnable, &QCheckBox::toggled, widget, [=](bool) { updateControls(); });
    QObject::connect(humiLow, &QCheckBox::toggled, widget, [=](bool) { updateControls(); });
    QObject::connect(humiHigh, &QCheckBox::toggled, widget, [=](bool) { updateControls(); });

    updateControls();
    return widget;
}

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
    if (deviceType == QStringLiteral("sensor_th")) {
        return createSensorThOptionsWidget(parent);
    }

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

bool DeviceTypeConfigRegistry::validateOptions(const QString &deviceType,
                                               QWidget *optionsWidget,
                                               QWidget *messageParent)
{
    if (deviceType != QStringLiteral("sensor_th") || !optionsWidget) {
        return true;
    }

    auto *thresholdEnable = findCheckBox(optionsWidget, kThresholdEnableName);
    if (!thresholdEnable || !thresholdEnable->isChecked()) {
        return true;
    }

    auto *tempEnable = findCheckBox(optionsWidget, kTempEnableName);
    auto *tempLow = findCheckBox(optionsWidget, kTempLowName);
    auto *tempHigh = findCheckBox(optionsWidget, kTempHighName);
    auto *tempLowSpin = findDoubleSpinBox(optionsWidget, kTempLowSpinName);
    auto *tempHighSpin = findDoubleSpinBox(optionsWidget, kTempHighSpinName);
    auto *humiEnable = findCheckBox(optionsWidget, kHumiEnableName);
    auto *humiLow = findCheckBox(optionsWidget, kHumiLowName);
    auto *humiHigh = findCheckBox(optionsWidget, kHumiHighName);
    auto *humiLowSpin = findDoubleSpinBox(optionsWidget, kHumiLowSpinName);
    auto *humiHighSpin = findDoubleSpinBox(optionsWidget, kHumiHighSpinName);

    if ((!tempEnable || !tempEnable->isChecked()) && (!humiEnable || !humiEnable->isChecked())) {
        QMessageBox::warning(messageParent, QStringLiteral("添加从站"), QStringLiteral("请至少选择一个阈值点"));
        return false;
    }

    const struct {
        const char *name;
        const QCheckBox *enableBox;
        const QCheckBox *lowBox;
        const QCheckBox *highBox;
        const QDoubleSpinBox *lowSpin;
        const QDoubleSpinBox *highSpin;
    } points[] = {
        {"温度", tempEnable, tempLow, tempHigh, tempLowSpin, tempHighSpin},
        {"湿度", humiEnable, humiLow, humiHigh, humiLowSpin, humiHighSpin},
    };

    for (const auto &point : points) {
        if (!point.enableBox || !point.enableBox->isChecked()) {
            continue;
        }
        if ((!point.lowBox || !point.lowBox->isChecked()) &&
            (!point.highBox || !point.highBox->isChecked())) {
            QMessageBox::warning(messageParent,
                                 QStringLiteral("添加从站"),
                                 QStringLiteral("%1 请至少选择低于或高于").arg(QString::fromUtf8(point.name)));
            return false;
        }
        if (point.lowBox && point.lowBox->isChecked() &&
            point.highBox && point.highBox->isChecked() &&
            point.lowSpin && point.highSpin &&
            point.lowSpin->value() >= point.highSpin->value()) {
            QMessageBox::warning(messageParent,
                                 QStringLiteral("添加从站"),
                                 QStringLiteral("%1 低阈值必须小于高阈值").arg(QString::fromUtf8(point.name)));
            return false;
        }
    }

    return true;
}

QVariantMap DeviceTypeConfigRegistry::collectOptions(const QString &deviceType, QWidget *optionsWidget)
{
    QVariantMap result;
    if (!optionsWidget) {
        return result;
    }

    if (deviceType == QStringLiteral("sensor_th")) {
        auto *thresholdEnable = findCheckBox(optionsWidget, kThresholdEnableName);
        if (!thresholdEnable || !thresholdEnable->isChecked()) {
            return result;
        }

        QVariantMap thresholds;
        thresholds.insert(QStringLiteral("temperature"),
                          pointThresholdPayload(findCheckBox(optionsWidget, kTempEnableName),
                                                findCheckBox(optionsWidget, kTempLowName),
                                                findDoubleSpinBox(optionsWidget, kTempLowSpinName),
                                                findCheckBox(optionsWidget, kTempHighName),
                                                findDoubleSpinBox(optionsWidget, kTempHighSpinName)));
        thresholds.insert(QStringLiteral("humidity"),
                          pointThresholdPayload(findCheckBox(optionsWidget, kHumiEnableName),
                                                findCheckBox(optionsWidget, kHumiLowName),
                                                findDoubleSpinBox(optionsWidget, kHumiLowSpinName),
                                                findCheckBox(optionsWidget, kHumiHighName),
                                                findDoubleSpinBox(optionsWidget, kHumiHighSpinName)));
        result.insert(QStringLiteral("threshold_enabled"), true);
        result.insert(QStringLiteral("thresholds"), thresholds);
        return result;
    }

    if (deviceType != QStringLiteral("relay")) {
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
