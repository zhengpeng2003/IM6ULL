#include "MyBtn.h"

MyBtn::MyBtn(const QString& offImg, const QString& onImg, QWidget* parent)
    : QPushButton(parent),
    m_offPixmap(offImg),
    m_onPixmap(onImg)
{
    setFlat(true);
    setCursor(Qt::PointingHandCursor);
    updateIcon(false);

    connect(this, &QPushButton::clicked, this, &MyBtn::onClicked);
}

void MyBtn::setState(bool on)
{
    m_currentState = on;
    updateIcon(on);
}
void MyBtn::updateIcon(bool on)
{
    setIcon(QIcon(on ? m_onPixmap : m_offPixmap));
    setIconSize(on ? m_onPixmap.size() : m_offPixmap.size());
}
void MyBtn::onClicked()  // 实现
{
    m_currentState = !m_currentState;
    updateIcon(m_currentState);
    emit gpioClicked(m_currentState);
}
