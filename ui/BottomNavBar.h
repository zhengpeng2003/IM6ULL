#pragma once
#include <QWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QPushButton>
#include "widget.h"
class BottomNavBar : public QWidget
{
    Q_OBJECT
public:
    explicit BottomNavBar(QWidget *parent);
signals:
    void sigPageChanged(int index);
private:
    void initUI();

};

