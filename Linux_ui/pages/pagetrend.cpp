#include "pagetrend.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QtMath>
#include <algorithm>

#include "pageui/TrendChartWidget.h"

PageTrend::PageTrend(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("PageArea");
    initUI();
    refreshControlState();
    queryCurrentTrend();
}

void PageTrend::setMasterList(const QList<MasterStatusInfo> &masters)
{
    const int oldSlot = selectedMasterSlot();
    masterInfos = masters;

    masterCombo->blockSignals(true);
    masterCombo->clear();
    masterCombo->addItem("请选择主站", -1);
    for (const MasterStatusInfo &master : masterInfos)
        masterCombo->addItem(master.masterName, master.masterSlot);

    int targetIndex = masterCombo->findData(oldSlot);
    if (targetIndex < 0)
        targetIndex = 0;
    masterCombo->setCurrentIndex(targetIndex);
    masterCombo->blockSignals(false);

    refreshSlaveCombo();
    refreshControlState();
    queryCurrentTrend();
}

void PageTrend::setSlaveList(const QList<SlaveDeviceInfo> &slaves)
{
    slaveInfos = slaves;
    for (int i = trendPoints.size() - 1; i >= 0; --i) {
        bool exists = false;
        for (const SlaveDeviceInfo &slave : slaveInfos) {
            if (trendPoints.at(i).masterSlot == slave.masterSlot &&
                trendPoints.at(i).slaveAddr == slave.slaveAddr &&
                trendPoints.at(i).deviceType == slave.deviceType) {
                exists = true;
                break;
            }
        }
        if (!exists)
            trendPoints.removeAt(i);
    }
    refreshSlaveCombo();
    refreshControlState();
    queryCurrentTrend();
}

void PageTrend::addData(const DataPack &pack)
{
    const QDateTime pointTime = pack.time.isValid()
        ? pack.time
        : QDateTime::currentDateTime();

    for (const DeviceData &device : pack.devices) {
        if (!device.valid)
            continue;

        const int masterSlot = masterSlotForDevice(device.deviceId, device.type);
        if (masterSlot < 0)
            continue;

        if (device.type == DEV_SENSOR_TH) {
            appendPoint(pointTime, masterSlot, device, "temperature", device.temperature);
            appendPoint(pointTime, masterSlot, device, "humidity", device.humidity);
        } else if (device.type == DEV_RELAY) {
            appendPoint(pointTime, masterSlot, device, "led", (device.relayStates & 0x01) ? 1.0 : 0.0);
            appendPoint(pointTime, masterSlot, device, "fan", (device.relayStates & 0x02) ? 1.0 : 0.0);
            appendPoint(pointTime, masterSlot, device, "buzzer", (device.relayStates & 0x04) ? 1.0 : 0.0);
        }
    }

    trimCache();
    queryCurrentTrend();
}

void PageTrend::initUI()
{



    QLabel *masterLabel = new QLabel("主站:", this);
    QLabel *slaveLabel = new QLabel("从站:", this);
    QLabel *curveLabel = new QLabel("曲线:", this);

    masterCombo = new QComboBox(this);
    slaveCombo = new QComboBox(this);
    curveCombo = new QComboBox(this);
    masterCombo->setObjectName("CompactCombo");
    slaveCombo->setObjectName("CompactCombo");
    curveCombo->setObjectName("CompactCombo");

    QHBoxLayout *selectRow = new QHBoxLayout;
    selectRow->setContentsMargins(0, 0, 0, 0);
    selectRow->setSpacing(4);
    selectRow->addWidget(masterLabel);
    selectRow->addWidget(masterCombo, 1);
    selectRow->addWidget(slaveLabel);
    selectRow->addWidget(slaveCombo, 1);
    selectRow->addWidget(curveLabel);
    selectRow->addWidget(curveCombo, 1);

    QLabel *timeLabel = new QLabel("时间:", this);
    timeRangeCombo = new QComboBox(this);
    timeRangeCombo->setObjectName("CompactCombo");
    timeRangeCombo->addItem("1小时", 3600);
    timeRangeCombo->addItem("6小时", 21600);
    timeRangeCombo->addItem("24小时", 86400);

    queryButton = new QPushButton("查询", this);
    queryButton->setObjectName("ActionButton");


    selectRow->setContentsMargins(0, 0, 0, 0);
    selectRow->setSpacing(4);
    selectRow->addStretch();
    selectRow->addWidget(timeLabel);
    selectRow->addWidget(timeRangeCombo);
    selectRow->addWidget(queryButton);

    chartWidget = new TrendChartWidget(this);
    chartWidget->setMinimumHeight(86);

    auto createStatCard = [this](const QString &title) {
        StatCard card;
        card.frame = new QFrame(this);
        card.frame->setObjectName("MetricCard");
        card.frame->setFixedHeight(34);

        card.title = new QLabel(title, card.frame);
        card.title->setObjectName("DetailKey");
        card.title->setAlignment(Qt::AlignCenter);

        card.value = new QLabel("--", card.frame);
        card.value->setObjectName("MetricValue");
        card.value->setAlignment(Qt::AlignCenter);

        QVBoxLayout *layout = new QVBoxLayout(card.frame);
        layout->setContentsMargins(4, 2, 4, 2);
        layout->setSpacing(0);
        layout->addWidget(card.title);
        layout->addWidget(card.value);
        return card;
    };

    latestCard = createStatCard("最新值");
    maxCard = createStatCard("最大值");
    minCard = createStatCard("最小值");
    avgCard = createStatCard("平均值");

    QHBoxLayout *statsRow = new QHBoxLayout;
    statsRow->setContentsMargins(0, 0, 0, 0);
    statsRow->setSpacing(4);
    statsRow->addWidget(latestCard.frame);
    statsRow->addWidget(maxCard.frame);
    statsRow->addWidget(minCard.frame);
    statsRow->addWidget(avgCard.frame);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(3);

    mainLayout->addLayout(selectRow);

    mainLayout->addWidget(chartWidget, 1);
    mainLayout->addLayout(statsRow);

    connect(masterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        refreshSlaveCombo();
        refreshControlState();
        queryCurrentTrend();
        emit masterChanged(selectedMasterSlot());
    });
    connect(slaveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        refreshCurveCombo();
        refreshControlState();
        queryCurrentTrend();
        const SlaveDeviceInfo slave = selectedSlave();
        emit slaveChanged(slave.masterSlot, slave.slaveAddr, slave.deviceType);
    });
    connect(curveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PageTrend::queryCurrentTrend);
    connect(timeRangeCombo, &QComboBox::currentTextChanged, this, [this](const QString &range) {
        emit timeRangeChanged(range);
    });
    connect(queryButton, &QPushButton::clicked, this, &PageTrend::queryCurrentTrend);
}

void PageTrend::refreshSlaveCombo()
{
    const int masterSlot = selectedMasterSlot();
    const int oldSlaveAddr = slaveCombo->currentData().toInt();

    slaveCombo->blockSignals(true);
    slaveCombo->clear();
    slaveCombo->addItem("请选择从站", -1);
    if (masterSlot >= 0) {
        for (const SlaveDeviceInfo &slave : slaveInfos) {
            if (slave.masterSlot != masterSlot)
                continue;
            const QString name = slave.displayName.isEmpty() ? slave.deviceName : slave.displayName;
            slaveCombo->addItem(QString("%1 %2").arg(slave.slaveAddr).arg(name),
                                slave.slaveAddr);
        }
    }

    int targetIndex = slaveCombo->findData(oldSlaveAddr);
    if (targetIndex < 0)
        targetIndex = 0;
    slaveCombo->setCurrentIndex(targetIndex);
    slaveCombo->blockSignals(false);

    refreshCurveCombo();
}

void PageTrend::refreshCurveCombo()
{
    const QString oldCurve = curveCombo->currentData().toString();
    const SlaveDeviceInfo slave = selectedSlave();
    const QList<CurveOption> options = curveOptionsForDevice(slave.deviceType);

    curveCombo->blockSignals(true);
    curveCombo->clear();
    for (const CurveOption &option : options)
        curveCombo->addItem(option.text, option.key);

    int targetIndex = curveCombo->findData(oldCurve);
    if (targetIndex < 0 && curveCombo->count() > 0)
        targetIndex = 0;
    if (targetIndex >= 0)
        curveCombo->setCurrentIndex(targetIndex);
    curveCombo->blockSignals(false);
}

void PageTrend::refreshControlState()
{
    const bool hasMaster = selectedMasterSlot() >= 0;
    const bool hasSlave = hasMaster && selectedSlave().slaveAddr > 0;
    const bool hasCurve = hasSlave && curveCombo->count() > 0;

    slaveCombo->setEnabled(hasMaster);
    curveCombo->setEnabled(hasSlave && hasCurve);
    timeRangeCombo->setEnabled(hasCurve);
    queryButton->setEnabled(hasCurve);
}

void PageTrend::queryCurrentTrend()
{
    const int masterSlot = selectedMasterSlot();
    const SlaveDeviceInfo slave = selectedSlave();
    const CurveOption curve = selectedCurve();
    const int seconds = rangeSeconds();
    const QDateTime endTime = QDateTime::currentDateTime();
    const QDateTime beginTime = endTime.addSecs(-seconds);

    QVector<double> values;
    for (const TrendPoint &point : trendPoints) {
        if (point.masterSlot != masterSlot ||
            point.slaveAddr != slave.slaveAddr ||
            point.deviceType != slave.deviceType ||
            point.curveKey != curve.key ||
            point.time < beginTime) {
            continue;
        }
        values.append(point.value);
    }

    chartWidget->setAxisLabels(makeAxisLabels(endTime, seconds));

    if (curve.key == "humidity") {
        chartWidget->setRange(0.0, 100.0);
    } else if (curve.key == "led" || curve.key == "fan" || curve.key == "buzzer") {
        chartWidget->setRange(0.0, 1.0);
    } else if (values.isEmpty()) {
        chartWidget->setRange(0.0, 100.0);
    } else {
        const auto minMax = std::minmax_element(values.constBegin(), values.constEnd());
        const double span = qMax(1.0, *minMax.second - *minMax.first);
        chartWidget->setRange(*minMax.first - span * 0.2, *minMax.second + span * 0.2);
    }

    chartWidget->setData(values, curve.unit);
    updateStats(values, curve.unit);
}

void PageTrend::updateStats(const QVector<double> &values, const QString &unit)
{
    if (values.isEmpty()) {
        setStatCard(latestCard, "--", QString());
        setStatCard(maxCard, "--", QString());
        setStatCard(minCard, "--", QString());
        setStatCard(avgCard, "--", QString());
        return;
    }

    const auto minMax = std::minmax_element(values.constBegin(), values.constEnd());
    double sum = 0.0;
    for (double value : values)
        sum += value;

    setStatCard(latestCard, QString("%1").arg(values.last(), 0, 'f', 1), unit);
    setStatCard(maxCard, QString("%1").arg(*minMax.second, 0, 'f', 1), unit);
    setStatCard(minCard, QString("%1").arg(*minMax.first, 0, 'f', 1), unit);
    setStatCard(avgCard, QString("%1").arg(sum / values.size(), 0, 'f', 1), unit);
}

void PageTrend::setStatCard(StatCard &card, const QString &value, const QString &unit)
{
    if (!card.value)
        return;
    card.value->setText(unit.isEmpty() || value == "--"
                            ? value
                            : QString("%1 %2").arg(value).arg(unit));
}

void PageTrend::appendPoint(const QDateTime &time,
                            int masterSlot,
                            const DeviceData &device,
                            const QString &curveKey,
                            double value)
{
    TrendPoint point;
    point.time = time;
    point.masterSlot = masterSlot;
    point.slaveAddr = device.deviceId;
    point.deviceType = deviceTypeString(device.type);
    point.curveKey = curveKey;
    point.value = value;
    trendPoints.append(point);
}

int PageTrend::selectedMasterSlot() const
{
    if (!masterCombo || masterCombo->currentIndex() < 0)
        return -1;
    return masterCombo->currentData().toInt();
}

SlaveDeviceInfo PageTrend::selectedSlave() const
{
    SlaveDeviceInfo empty;
    const int masterSlot = selectedMasterSlot();
    const int slaveAddr = slaveCombo ? slaveCombo->currentData().toInt() : -1;
    if (masterSlot < 0 || slaveAddr <= 0)
        return empty;

    for (const SlaveDeviceInfo &slave : slaveInfos) {
        if (slave.masterSlot == masterSlot && slave.slaveAddr == slaveAddr)
            return slave;
    }
    return empty;
}

PageTrend::CurveOption PageTrend::selectedCurve() const
{
    const QString key = curveCombo ? curveCombo->currentData().toString() : QString();
    const QList<CurveOption> options = curveOptionsForDevice(selectedSlave().deviceType);
    for (const CurveOption &option : options) {
        if (option.key == key)
            return option;
    }
    return CurveOption();
}

QList<PageTrend::CurveOption> PageTrend::curveOptionsForDevice(const QString &deviceType) const
{
    QList<CurveOption> options;
    if (deviceType == "sensor_th") {
        options.append(CurveOption{"temperature", "温度趋势", "℃"});
        options.append(CurveOption{"humidity", "湿度趋势", "%"});
    } else if (deviceType == "relay") {
        options.append(CurveOption{"led", "LED状态", ""});
        options.append(CurveOption{"fan", "FAN状态", ""});
        options.append(CurveOption{"buzzer", "BUZZER状态", ""});
    } else if (deviceType == "meter") {
        options.append(CurveOption{"voltage", "电压趋势", "V"});
        options.append(CurveOption{"current", "电流趋势", "A"});
        options.append(CurveOption{"power", "功率趋势", "W"});
        options.append(CurveOption{"energy", "电能趋势", "kWh"});
    }
    return options;
}

int PageTrend::rangeSeconds() const
{
    if (!timeRangeCombo)
        return 3600;
    const int value = timeRangeCombo->currentData().toInt();
    return value > 0 ? value : 3600;
}

QStringList PageTrend::makeAxisLabels(const QDateTime &endTime, int seconds) const
{
    QStringList labels;
    const QDateTime beginTime = endTime.addSecs(-seconds);
    for (int i = 0; i < 10; ++i) {
        const int offset = qRound(seconds * (i / 9.0));
        labels.append(beginTime.addSecs(offset).toString("HH:mm"));
    }
    return labels;
}

int PageTrend::masterSlotForDevice(int slaveAddr, DeviceType type) const
{
    const QString deviceType = deviceTypeString(type);
    for (const SlaveDeviceInfo &slave : slaveInfos) {
        if (slave.slaveAddr == slaveAddr && slave.deviceType == deviceType)
            return slave.masterSlot;
    }

    if (masterInfos.size() == 1)
        return masterInfos.first().masterSlot;
    return -1;
}

QString PageTrend::deviceTypeString(DeviceType type) const
{
    if (type == DEV_SENSOR_TH)
        return "sensor_th";
    if (type == DEV_RELAY)
        return "relay";
    if (type == DEV_METER)
        return "meter";
    return "unknown";
}

void PageTrend::trimCache()
{
    while (trendPoints.size() > maxCachePoints)
        trendPoints.removeFirst();
}
