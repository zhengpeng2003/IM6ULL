#ifndef TEMPINFOWIDGET_H
#define TEMPINFOWIDGET_H

#include <QWidget>

class QLabel;

class TempInfoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TempInfoWidget(QWidget *parent = nullptr);

    void setTemperature(float t);
    void setHumidity(float h);

private:
    QLabel *titleLabel;
    QLabel *tempLabel;
    QLabel *humiLabel;
};

#endif
