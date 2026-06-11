#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QString>

class QLabel;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QPushButton;
class QShowEvent;
class QSpinBox;
class LoadingSpinnerWidget;

class AddSlaveDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddSlaveDialog(int masterSlot, QWidget *parent = nullptr);

    int masterSlot() const;
    int slaveId() const;
    QString deviceType() const;
    int pollIntervalMs() const;
    QJsonObject thresholdPayload() const;

    void setAdding();
    void setResult(bool ok, const QString &text);

signals:
    void addSlaveRequested(int masterSlot,
                           int slaveId,
                           const QString &deviceType,
                           int pollIntervalMs,
                           const QJsonObject &thresholdPayload);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void initUI();
    void setFormEnabled(bool enabled);
    void updateThresholdVisibility();
    void updateThresholdControls();
    bool validateThresholdConfig(QString *message) const;
    QJsonObject pointThresholdPayload(const QCheckBox *enableBox,
                                      const QCheckBox *lowBox,
                                      const QDoubleSpinBox *lowSpin,
                                      const QCheckBox *highBox,
                                      const QDoubleSpinBox *highSpin) const;

    int m_masterSlot = 0;
    bool m_formEnabled = true;
    QLabel *m_statusLabel = nullptr;
    QSpinBox *m_slaveIdSpin = nullptr;
    QComboBox *m_deviceTypeCombo = nullptr;
    QSpinBox *m_pollIntervalSpin = nullptr;
    QFrame *m_thresholdPanel = nullptr;
    QCheckBox *m_thresholdEnableBox = nullptr;
    QCheckBox *m_tempEnableBox = nullptr;
    QCheckBox *m_tempLowBox = nullptr;
    QDoubleSpinBox *m_tempLowSpin = nullptr;
    QCheckBox *m_tempHighBox = nullptr;
    QDoubleSpinBox *m_tempHighSpin = nullptr;
    QCheckBox *m_humiEnableBox = nullptr;
    QCheckBox *m_humiLowBox = nullptr;
    QDoubleSpinBox *m_humiLowSpin = nullptr;
    QCheckBox *m_humiHighBox = nullptr;
    QDoubleSpinBox *m_humiHighSpin = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    LoadingSpinnerWidget *m_spinner = nullptr;
};
