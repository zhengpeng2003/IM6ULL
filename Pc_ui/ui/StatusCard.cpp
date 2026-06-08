#include "StatusCard.h"
#include <QVBoxLayout>
#include <QLabel>

StatusCard::StatusCard(const QString &title, QWidget *parent) : QWidget(parent)
{
    setObjectName("StatusCard");
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(6);

    m_title = new QLabel(title, this);
    m_title->setObjectName("CardTitle");
    m_value = new QLabel("0", this);
    m_value->setObjectName("CardValue");
    m_subText = new QLabel(QStringLiteral("实时统计"), this);
    m_subText->setObjectName("CardSubText");

    layout->addWidget(m_title);
    layout->addWidget(m_value);
    layout->addWidget(m_subText);
}

void StatusCard::setValue(const QString &value)
{
    m_value->setText(value);
}

void StatusCard::setSubText(const QString &text)
{
    m_subText->setText(text);
}
