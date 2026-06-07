#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QComboBox;
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

    void setAdding();
    void setResult(bool ok, const QString &text);

signals:
    void addSlaveRequested(int masterSlot,
                           int slaveId,
                           const QString &deviceType,
                           int pollIntervalMs);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void initUI();
    void setFormEnabled(bool enabled);

    int m_masterSlot = 0;
    QLabel *m_statusLabel = nullptr;
    QSpinBox *m_slaveIdSpin = nullptr;
    QComboBox *m_deviceTypeCombo = nullptr;
    QSpinBox *m_pollIntervalSpin = nullptr;
    QPushButton *m_addButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    LoadingSpinnerWidget *m_spinner = nullptr;
};
