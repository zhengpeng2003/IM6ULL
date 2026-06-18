#pragma once
#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QTimer>

class TopStatusBar : public QWidget
{
    Q_OBJECT
public:
    explicit TopStatusBar(QWidget *parent = nullptr);
    void setBackendConnected(bool connected);
    void setConfigSyncState(const QString &status,
                            const QString &reason,
                            const QString &message,
                            int retryCount);

private:
    QString textForConfigSyncState(const QString &status,
                                   const QString &reason,
                                   const QString &message,
                                   int retryCount) const;
    void initUI();
    void initSignal();

private:
    QLabel *titleLabel;
    QLabel *timeLabel;
    QLabel *statusDot;
};
