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

private:
    void initUI();
    void initSignal();

private:
    QLabel *titleLabel;
    QLabel *timeLabel;
    QLabel *statusDot;
};

