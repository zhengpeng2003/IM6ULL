#include "addslavedialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QShowEvent>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include "ui/loadingspinnerwidget.h"

AddSlaveDialog::AddSlaveDialog(int masterSlot, QWidget *parent)
    : QDialog(parent)
    , m_masterSlot(masterSlot)
{
    setObjectName("PageArea");
    setModal(true);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setFixedSize(360, 264);
    initUI();
}

int AddSlaveDialog::masterSlot() const
{
    return m_masterSlot;
}

int AddSlaveDialog::slaveId() const
{
    return m_slaveIdSpin->value();
}

QString AddSlaveDialog::deviceType() const
{
    return m_deviceTypeCombo->currentData().toString();
}

int AddSlaveDialog::pollIntervalMs() const
{
    return m_pollIntervalSpin->value();
}

QJsonObject AddSlaveDialog::thresholdPayload() const
{
    QJsonObject payload;
    if (deviceType() != "sensor_th" ||
        !m_thresholdEnableBox ||
        !m_thresholdEnableBox->isChecked()) {
        return payload;
    }

    QJsonObject thresholds;
    thresholds.insert("temperature",
                      pointThresholdPayload(m_tempEnableBox,
                                            m_tempLowBox,
                                            m_tempLowSpin,
                                            m_tempHighBox,
                                            m_tempHighSpin));
    thresholds.insert("humidity",
                      pointThresholdPayload(m_humiEnableBox,
                                            m_humiLowBox,
                                            m_humiLowSpin,
                                            m_humiHighBox,
                                            m_humiHighSpin));

    payload.insert("threshold_enabled", true);
    payload.insert("thresholds", thresholds);
    return payload;
}

void AddSlaveDialog::setAdding()
{
    setFormEnabled(false);
    m_spinner->start();
    m_statusLabel->setText("Adding slave...");
}

void AddSlaveDialog::setResult(bool ok, const QString &text)
{
    m_spinner->stop();
    m_statusLabel->setText(text);

    if (ok) {
        m_cancelButton->setEnabled(false);
        m_addButton->setEnabled(false);
        QTimer::singleShot(700, this, &QDialog::accept);
    } else {
        setFormEnabled(true);
    }
}

void AddSlaveDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);

    if (!parentWidget())
        return;

    const QPoint parentCenter = parentWidget()->geometry().center();
    move(parentCenter.x() - width() / 2, parentCenter.y() - height() / 2);
}

void AddSlaveDialog::initUI()
{
    QLabel *titleLabel = new QLabel("Add Slave", this);
    titleLabel->setObjectName("PanelTitle");

    QLabel *masterLabel = new QLabel(QString("Master %1   Slot:%2")
                                         .arg(m_masterSlot + 1)
                                         .arg(m_masterSlot),
                                     this);
    masterLabel->setObjectName("HintText");

    m_slaveIdSpin = new QSpinBox(this);
    m_slaveIdSpin->setObjectName("CompactLineEdit");
    m_slaveIdSpin->setRange(1, 247);
    m_slaveIdSpin->setValue(1);

    m_deviceTypeCombo = new QComboBox(this);
    m_deviceTypeCombo->setObjectName("CompactCombo");
    m_deviceTypeCombo->addItem("Temp/Humidity", "sensor_th");
    m_deviceTypeCombo->addItem("Relay", "relay");

    m_pollIntervalSpin = new QSpinBox(this);
    m_pollIntervalSpin->setObjectName("CompactLineEdit");
    m_pollIntervalSpin->setRange(500, 60000);
    m_pollIntervalSpin->setSingleStep(100);
    m_pollIntervalSpin->setSuffix(" ms");
    m_pollIntervalSpin->setValue(1000);

    QFormLayout *formLayout = new QFormLayout;
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(5);
    formLayout->addRow("Slave ID:", m_slaveIdSpin);
    formLayout->addRow("Type:", m_deviceTypeCombo);
    formLayout->addRow("Poll:", m_pollIntervalSpin);

    m_thresholdPanel = new QFrame(this);
    m_thresholdPanel->setObjectName("Panel");

    m_thresholdEnableBox = new QCheckBox("Enable thresholds", m_thresholdPanel);
    m_thresholdEnableBox->setObjectName("DetailValue");

    m_tempEnableBox = new QCheckBox("Temp", m_thresholdPanel);
    m_tempLowBox = new QCheckBox("L", m_thresholdPanel);
    m_tempHighBox = new QCheckBox("H", m_thresholdPanel);
    m_humiEnableBox = new QCheckBox("Humi", m_thresholdPanel);
    m_humiLowBox = new QCheckBox("L", m_thresholdPanel);
    m_humiHighBox = new QCheckBox("H", m_thresholdPanel);

    const QList<QCheckBox *> thresholdChecks = {
        m_tempEnableBox, m_tempLowBox, m_tempHighBox,
        m_humiEnableBox, m_humiLowBox, m_humiHighBox
    };
    for (QCheckBox *box : thresholdChecks)
        box->setObjectName("DetailKey");

    auto createThresholdSpin = [this](double value, const QString &suffix) {
        QDoubleSpinBox *spin = new QDoubleSpinBox(m_thresholdPanel);
        spin->setObjectName("CompactLineEdit");
        spin->setRange(-1000.0, 1000.0);
        spin->setDecimals(1);
        spin->setSingleStep(1.0);
        spin->setValue(value);
        spin->setSuffix(suffix);
        spin->setFixedWidth(64);
        return spin;
    };

    m_tempLowSpin = createThresholdSpin(10.0, " C");
    m_tempHighSpin = createThresholdSpin(35.0, " C");
    m_humiLowSpin = createThresholdSpin(20.0, " %");
    m_humiHighSpin = createThresholdSpin(80.0, " %");

    QHBoxLayout *tempLayout = new QHBoxLayout;
    tempLayout->setContentsMargins(0, 0, 0, 0);
    tempLayout->setSpacing(3);
    tempLayout->addWidget(m_tempEnableBox);
    tempLayout->addStretch();
    tempLayout->addWidget(m_tempLowBox);
    tempLayout->addWidget(m_tempLowSpin);
    tempLayout->addWidget(m_tempHighBox);
    tempLayout->addWidget(m_tempHighSpin);

    QHBoxLayout *humiLayout = new QHBoxLayout;
    humiLayout->setContentsMargins(0, 0, 0, 0);
    humiLayout->setSpacing(3);
    humiLayout->addWidget(m_humiEnableBox);
    humiLayout->addStretch();
    humiLayout->addWidget(m_humiLowBox);
    humiLayout->addWidget(m_humiLowSpin);
    humiLayout->addWidget(m_humiHighBox);
    humiLayout->addWidget(m_humiHighSpin);

    QVBoxLayout *thresholdLayout = new QVBoxLayout(m_thresholdPanel);
    thresholdLayout->setContentsMargins(6, 4, 6, 4);
    thresholdLayout->setSpacing(3);
    thresholdLayout->addWidget(m_thresholdEnableBox);
    thresholdLayout->addLayout(tempLayout);
    thresholdLayout->addLayout(humiLayout);

    m_spinner = new LoadingSpinnerWidget(this);
    m_spinner->setColors(QColor(0, 191, 165), QColor(95, 110, 116));

    m_statusLabel = new QLabel("Ready", this);
    m_statusLabel->setObjectName("HintText");

    QHBoxLayout *statusLayout = new QHBoxLayout;
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(6);
    statusLayout->addWidget(m_spinner);
    statusLayout->addWidget(m_statusLabel, 1);

    m_cancelButton = new QPushButton("Cancel", this);
    m_cancelButton->setObjectName("GhostButton");
    m_cancelButton->setFixedWidth(72);

    m_addButton = new QPushButton("Add", this);
    m_addButton->setObjectName("ActionButton");
    m_addButton->setFixedWidth(72);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(6);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_addButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 7, 10, 7);
    mainLayout->setSpacing(5);
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(masterLabel);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_thresholdPanel);
    mainLayout->addLayout(statusLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        QString message;
        if (!validateThresholdConfig(&message)) {
            m_statusLabel->setText(message);
            return;
        }

        emit addSlaveRequested(masterSlot(),
                               slaveId(),
                               deviceType(),
                               pollIntervalMs(),
                               thresholdPayload());
    });

    connect(m_deviceTypeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this,
            &AddSlaveDialog::updateThresholdVisibility);
    connect(m_thresholdEnableBox, &QCheckBox::toggled, this, &AddSlaveDialog::updateThresholdControls);
    connect(m_tempEnableBox, &QCheckBox::toggled, this, &AddSlaveDialog::updateThresholdControls);
    connect(m_tempLowBox, &QCheckBox::toggled, this, &AddSlaveDialog::updateThresholdControls);
    connect(m_tempHighBox, &QCheckBox::toggled, this, &AddSlaveDialog::updateThresholdControls);
    connect(m_humiEnableBox, &QCheckBox::toggled, this, &AddSlaveDialog::updateThresholdControls);
    connect(m_humiLowBox, &QCheckBox::toggled, this, &AddSlaveDialog::updateThresholdControls);
    connect(m_humiHighBox, &QCheckBox::toggled, this, &AddSlaveDialog::updateThresholdControls);

    updateThresholdVisibility();
}

void AddSlaveDialog::setFormEnabled(bool enabled)
{
    m_formEnabled = enabled;
    m_slaveIdSpin->setEnabled(enabled);
    m_deviceTypeCombo->setEnabled(enabled);
    m_pollIntervalSpin->setEnabled(enabled);
    m_addButton->setEnabled(enabled);
    m_cancelButton->setEnabled(enabled);
    updateThresholdControls();
}

void AddSlaveDialog::updateThresholdVisibility()
{
    const bool showThresholds = deviceType() == "sensor_th";
    if (m_thresholdPanel)
        m_thresholdPanel->setVisible(showThresholds);
    updateThresholdControls();
}

void AddSlaveDialog::updateThresholdControls()
{
    const bool available = m_formEnabled && deviceType() == "sensor_th";
    const bool enabled = available &&
                         m_thresholdEnableBox &&
                         m_thresholdEnableBox->isChecked();
    const bool tempEnabled = enabled && m_tempEnableBox->isChecked();
    const bool humiEnabled = enabled && m_humiEnableBox->isChecked();

    if (m_thresholdEnableBox)
        m_thresholdEnableBox->setEnabled(available);

    const QList<QCheckBox *> pointEnableBoxes = {m_tempEnableBox, m_humiEnableBox};
    for (QCheckBox *box : pointEnableBoxes)
        box->setEnabled(enabled);

    m_tempLowBox->setEnabled(tempEnabled);
    m_tempHighBox->setEnabled(tempEnabled);
    m_tempLowSpin->setEnabled(tempEnabled && m_tempLowBox->isChecked());
    m_tempHighSpin->setEnabled(tempEnabled && m_tempHighBox->isChecked());

    m_humiLowBox->setEnabled(humiEnabled);
    m_humiHighBox->setEnabled(humiEnabled);
    m_humiLowSpin->setEnabled(humiEnabled && m_humiLowBox->isChecked());
    m_humiHighSpin->setEnabled(humiEnabled && m_humiHighBox->isChecked());
}

bool AddSlaveDialog::validateThresholdConfig(QString *message) const
{
    if (deviceType() != "sensor_th" || !m_thresholdEnableBox->isChecked())
        return true;

    if (!m_tempEnableBox->isChecked() && !m_humiEnableBox->isChecked()) {
        if (message)
            *message = "Select threshold point";
        return false;
    }

    const struct {
        const char *name;
        const QCheckBox *enableBox;
        const QCheckBox *lowBox;
        const QDoubleSpinBox *lowSpin;
        const QCheckBox *highBox;
        const QDoubleSpinBox *highSpin;
    } points[] = {
        {"Temp", m_tempEnableBox, m_tempLowBox, m_tempLowSpin, m_tempHighBox, m_tempHighSpin},
        {"Humi", m_humiEnableBox, m_humiLowBox, m_humiLowSpin, m_humiHighBox, m_humiHighSpin},
    };

    for (const auto &point : points) {
        if (!point.enableBox->isChecked())
            continue;

        if (!point.lowBox->isChecked() && !point.highBox->isChecked()) {
            if (message)
                *message = QString("%1 needs L or H").arg(point.name);
            return false;
        }

        if (point.lowBox->isChecked() &&
            point.highBox->isChecked() &&
            point.lowSpin->value() > point.highSpin->value()) {
            if (message)
                *message = QString("%1 L > H").arg(point.name);
            return false;
        }
    }

    return true;
}

QJsonObject AddSlaveDialog::pointThresholdPayload(const QCheckBox *enableBox,
                                                  const QCheckBox *lowBox,
                                                  const QDoubleSpinBox *lowSpin,
                                                  const QCheckBox *highBox,
                                                  const QDoubleSpinBox *highSpin) const
{
    QJsonObject point;
    const bool enabled = enableBox && enableBox->isChecked();
    point.insert("enable_alarm", enabled);

    if (enabled && lowBox && lowBox->isChecked() && lowSpin)
        point.insert("alarm_low", lowSpin->value());
    if (enabled && highBox && highBox->isChecked() && highSpin)
        point.insert("alarm_high", highSpin->value());

    return point;
}
