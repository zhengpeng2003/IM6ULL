#ifndef GPIOWIDGET_H
#define GPIOWIDGET_H

#include <QWidget>
#include <QVector>
#include <QDebug>
#include "TempInfoWidget.h"
#include "MainWidget.h"
class MyBtn;
class QGridLayout;

namespace Ui {
class GpioWidget;
}

class GpioWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GpioWidget(QWidget *parent = nullptr);
    ~GpioWidget();

private slots:
    void onExitButtonClicked();
public slots:
    void tempupdate(double temp,double humi);
private:
    void InitMyBtns();
    void InitLayout();

    Ui::GpioWidget *ui;
    QGridLayout *m_gridLayout;
    TempInfoWidget *tempWidget;
    QVector<MyBtn*> m_buttons;
};

#endif // GPIOWIDGET_H
