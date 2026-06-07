#include "addslavedialog.h"

#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
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
    setFixedSize(330, 214);
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
    formLayout->setSpacing(6);
    formLayout->addRow("Slave ID:", m_slaveIdSpin);
    formLayout->addRow("Type:", m_deviceTypeCombo);
    formLayout->addRow("Poll:", m_pollIntervalSpin);

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
    mainLayout->setContentsMargins(12, 10, 12, 10);
    mainLayout->setSpacing(8);
    mainLayout->addWidget(titleLabel);
    mainLayout->addWidget(masterLabel);
    mainLayout->addLayout(formLayout);
    mainLayout->addLayout(statusLayout);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_addButton, &QPushButton::clicked, this, [this]() {
        emit addSlaveRequested(masterSlot(), slaveId(), deviceType(), pollIntervalMs());
    });
}

void AddSlaveDialog::setFormEnabled(bool enabled)
{
    m_slaveIdSpin->setEnabled(enabled);
    m_deviceTypeCombo->setEnabled(enabled);
    m_pollIntervalSpin->setEnabled(enabled);
    m_addButton->setEnabled(enabled);
    m_cancelButton->setEnabled(enabled);
}
