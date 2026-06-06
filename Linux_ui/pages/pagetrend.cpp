#include "pagetrend.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <algorithm>

#include "TrendChartWidget.h"

PageTrend::PageTrend(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("PageArea");
    initUI();
    switchTrendMode(TemperatureMode);
}

void PageTrend::setMasterList(const QStringList &masters)
{
    masterCombo->blockSignals(true);
    masterCombo->clear();
    masterCombo->addItems(masters);
    masterCombo->blockSignals(false);
}

void PageTrend::setSlaveList(const QStringList &slaves)
{
    slaveCombo->blockSignals(true);
    slaveCombo->clear();
    slaveCombo->addItems(slaves);
    slaveCombo->blockSignals(false);
}

void PageTrend::appendTemperature(double value)
{
    temperatureValues.append(value);
    trimData(temperatureValues);
    if (currentMode == TemperatureMode)
        updateChart();
}

void PageTrend::appendHumidity(double value)
{
    humidityValues.append(value);
    trimData(humidityValues);
    if (currentMode == HumidityMode)
        updateChart();
}

void PageTrend::setTemperatureData(const QVector<double> &values)
{
    temperatureValues = values;
    trimData(temperatureValues);
    if (currentMode == TemperatureMode)
        updateChart();
}

void PageTrend::setHumidityData(const QVector<double> &values)
{
    humidityValues = values;
    trimData(humidityValues);
    if (currentMode == HumidityMode)
        updateChart();
}

void PageTrend::addData(const DataPack &pack)
{
    for (const DeviceData &device : pack.devices) {
        if (device.type != DEV_SENSOR_TH || !device.valid)
            continue;

        appendTemperature(device.temperature);
        appendHumidity(device.humidity);
    }
}

void PageTrend::initUI()
{
    QLabel *titleLabel = new QLabel("Trend Analysis", this);
    titleLabel->setObjectName("PanelTitle");
    titleLabel->setFixedHeight(20);

    QLabel *masterLabel = new QLabel("Master:", this);
    QLabel *slaveLabel = new QLabel("Slave:", this);

    masterCombo = new QComboBox(this);
    slaveCombo = new QComboBox(this);
    masterCombo->setObjectName("CompactCombo");
    slaveCombo->setObjectName("CompactCombo");

    QHBoxLayout *selectRow = new QHBoxLayout;
    selectRow->setContentsMargins(0, 0, 0, 0);
    selectRow->setSpacing(4);
    selectRow->addWidget(masterLabel);
    selectRow->addWidget(masterCombo, 1);
    selectRow->addWidget(slaveLabel);
    selectRow->addWidget(slaveCombo, 1);

    temperatureButton = new QPushButton("Temperature", this);
    humidityButton = new QPushButton("Humidity", this);
    temperatureButton->setObjectName("ToggleButton");
    humidityButton->setObjectName("ToggleButton");

    QLabel *timeLabel = new QLabel("Range:", this);
    timeRangeCombo = new QComboBox(this);
    timeRangeCombo->setObjectName("CompactCombo");
    timeRangeCombo->addItem("5 min");
    timeRangeCombo->addItem("10 min");
    timeRangeCombo->addItem("30 min");

    QHBoxLayout *modeRow = new QHBoxLayout;
    modeRow->setContentsMargins(0, 0, 0, 0);
    modeRow->setSpacing(4);
    modeRow->addWidget(temperatureButton);
    modeRow->addWidget(humidityButton);
    modeRow->addStretch();
    modeRow->addWidget(timeLabel);
    modeRow->addWidget(timeRangeCombo);

    chartWidget = new TrendChartWidget(this);
    chartWidget->setMinimumHeight(82);

    statsLabel = new QLabel(this);
    statsLabel->setObjectName("StatsBar");
    statsLabel->setFixedHeight(22);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(4);
    mainLayout->addWidget(titleLabel);
    mainLayout->addLayout(selectRow);
    mainLayout->addLayout(modeRow);
    mainLayout->addWidget(chartWidget, 1);
    mainLayout->addWidget(statsLabel);

    connect(temperatureButton, &QPushButton::clicked, this, [this]() {
        switchTrendMode(TemperatureMode);
    });
    connect(humidityButton, &QPushButton::clicked, this, [this]() {
        switchTrendMode(HumidityMode);
    });
    connect(masterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PageTrend::masterChanged);
    connect(slaveCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PageTrend::slaveChanged);
    connect(timeRangeCombo, &QComboBox::currentTextChanged,
            this, &PageTrend::timeRangeChanged);
}

void PageTrend::switchTrendMode(TrendMode mode)
{
    currentMode = mode;
    updateModeButtons();
    updateChart();
}

void PageTrend::updateModeButtons()
{
    temperatureButton->setProperty("checked", currentMode == TemperatureMode);
    humidityButton->setProperty("checked", currentMode == HumidityMode);
    temperatureButton->style()->unpolish(temperatureButton);
    temperatureButton->style()->polish(temperatureButton);
    humidityButton->style()->unpolish(humidityButton);
    humidityButton->style()->polish(humidityButton);
}

void PageTrend::updateChart()
{
    if (currentMode == TemperatureMode) {
        chartWidget->setRange(24.0, 30.0);
        chartWidget->setData(temperatureValues, "C");
    } else {
        chartWidget->setRange(55.0, 70.0);
        chartWidget->setData(humidityValues, "%");
    }
    updateStats();
}

void PageTrend::updateStats()
{
    const QVector<double> &values = currentMode == TemperatureMode ? temperatureValues : humidityValues;
    const QString unit = currentMode == TemperatureMode ? "C" : "%";
    if (values.isEmpty()) {
        statsLabel->setText("Now:--  Max:--  Min:--  Avg:--");
        return;
    }

    const auto minMax = std::minmax_element(values.constBegin(), values.constEnd());
    double sum = 0.0;
    for (double value : values)
        sum += value;

    statsLabel->setText(QString("Now:%1%2  Max:%3%2  Min:%4%2  Avg:%5%2")
                            .arg(values.last(), 0, 'f', 1)
                            .arg(unit)
                            .arg(*minMax.second, 0, 'f', 1)
                            .arg(*minMax.first, 0, 'f', 1)
                            .arg(sum / values.size(), 0, 'f', 1));
}

void PageTrend::trimData(QVector<double> &values)
{
    while (values.size() > maxPoints)
        values.removeFirst();
}
