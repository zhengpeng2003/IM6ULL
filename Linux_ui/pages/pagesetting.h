#pragma once

#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QPushButton>
#include <QWidget>

class QComboBox;
class QVBoxLayout;

struct MasterPortInfo {
    int masterSlot = 0;
    QString masterName;
    QString deviceNode;
    QString areaName;
    int baudRate = 9600;
    bool connected = false;
};

class PageSetting : public QWidget
{
    Q_OBJECT
public:
    explicit PageSetting(QWidget *parent = nullptr);

    void setUnscannedState();
    void setPortList(const QList<MasterPortInfo> &ports);
    void updateMasterConnectionState(int masterSlot, bool connected);

signals:
    void scanPortsRequested();
    void connectMasterRequested(int masterSlot,
                                const QString &deviceNode,
                                const QString &areaName,
                                int baudRate);
    void disconnectMasterRequested(int masterSlot, const QString &deviceNode);

private:
    struct PortCard {
        QFrame *frame = nullptr;
        QLabel *stateLabel = nullptr;
        QPushButton *actionButton = nullptr;
        QLineEdit *areaEdit = nullptr;
        QComboBox *baudCombo = nullptr;
        MasterPortInfo info;
    };

    void initUI();
    void clearPortCards();
    QFrame *createPortCard(const MasterPortInfo &info);
    void updateCardState(PortCard &card, bool connected);

    QPushButton *scanButton = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *emptyTitleLabel = nullptr;
    QLabel *emptyHintLabel = nullptr;
    QWidget *emptyWidget = nullptr;
    QWidget *cardsWidget = nullptr;
    QVBoxLayout *cardsLayout = nullptr;
    QList<PortCard *> portCards;
};
