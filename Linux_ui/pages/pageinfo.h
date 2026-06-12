#ifndef PAGEINFO_H
#define PAGEINFO_H

#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QVector>
#include <QWidget>

#include "data/data_protocol.h"

class Pageinfo : public QWidget
{
    Q_OBJECT
public:
    explicit Pageinfo(QWidget *parent = nullptr);
    void addInfo(const DataPack &pack);
    void setIpcConnected(bool connected);
    void updateOfflineCacheStatus(bool cacheEnabled, bool flushEnabled, int pendingCount);
    void setOfflineCacheUnsupported();

signals:
    void reconnectIpcRequested();
    void offlineCacheConfigChanged(bool cacheEnabled, bool flushEnabled);
    void clearOfflineCacheRequested();
    void flushOfflineCacheRequested();

private:
    struct InfoCardSpec {
        QString key;
        QString title;
        QString defaultValue;
        QString defaultState;
    };

    struct InfoCardWidget {
        QString key;
        QFrame *frame = nullptr;
        QLabel *iconLabel = nullptr;
        QLabel *titleLabel = nullptr;
        QLabel *valueLabel = nullptr;
    };

    struct RuntimeRowSpec {
        QString key;
        QString title;
        QString defaultValue;
        QString defaultState;
    };

    struct RuntimeRowWidget {
        QString key;
        QLabel *dotLabel = nullptr;
        QLabel *titleLabel = nullptr;
        QLabel *valueLabel = nullptr;
    };

    void initUI();
    QFrame *createInfoCard(const InfoCardSpec &spec);
    QWidget *createRuntimeRow(const RuntimeRowSpec &spec);
    void setInfoCardValue(const QString &key, const QString &value, const QString &state = QString());
    void setRuntimeRowValue(const QString &key, const QString &value, const QString &state = QString());
    void polishState(QWidget *widget);
    void updateIpcStatusLabel();
    void updateLastSync(const QDateTime &time);
    void updateOfflineCacheButtons();
    void setToggleButtonChecked(QPushButton *button, bool checked);

    QLabel *hintLabel = nullptr;
    QPushButton *reconnectButton = nullptr;
    QLabel *cacheStateLabel = nullptr;
    QLabel *flushStateLabel = nullptr;
    QLabel *pendingCountLabel = nullptr;
    QPushButton *cacheEnableButton = nullptr;
    QPushButton *flushEnableButton = nullptr;
    QPushButton *clearCacheButton = nullptr;
    QPushButton *flushCacheButton = nullptr;

    QVector<InfoCardWidget> infoCards;
    QVector<RuntimeRowWidget> runtimeRows;
    QDateTime lastSyncTime;
    bool m_ipcConnected = false;
    bool m_cacheEnabled = false;
    bool m_flushEnabled = false;
    bool m_cacheSupported = true;
    int m_pendingCount = 0;
};

#endif // PAGEINFO_H
