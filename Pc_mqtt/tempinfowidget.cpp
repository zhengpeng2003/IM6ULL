#include "TempInfoWidget.h"
#include <QVBoxLayout>
#include <QLabel>

TempInfoWidget::TempInfoWidget(QWidget *parent)
    : QWidget(parent)
{
    setFixedSize(150, 80);
    setStyleSheet(
        "QWidget {"
        " border: 2px solid #666;"
        " border-radius: 8px;"
        " background: #222;"
        " color: white;"
        "}"
        );

    titleLabel = new QLabel("TEMP / HUMI", this);
    tempLabel  = new QLabel("Temp: -- <sup>o</sup>C", this);
    humiLabel  = new QLabel("Humi: -- %", this);

    titleLabel->setAlignment(Qt::AlignCenter);
    tempLabel->setAlignment(Qt::AlignCenter);
    humiLabel->setAlignment(Qt::AlignCenter);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1,1,1,1);
    layout->addWidget(titleLabel);
    layout->addWidget(tempLabel);
    layout->addWidget(humiLabel);
}

void TempInfoWidget::setTemperature(float t)
{
    tempLabel->setText(
        QString("Temp: %1 <sup>o</sup>C").arg(t, 0, 'f', 1)
        );
    tempLabel->setTextFormat(Qt::RichText);

}

void TempInfoWidget::setHumidity(float h)
{
    humiLabel->setText(QString("Humi: %1 %").arg(h, 0, 'f', 1));
}
