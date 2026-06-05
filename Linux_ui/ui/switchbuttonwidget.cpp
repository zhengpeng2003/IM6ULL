#include "switchbuttonwidget.h"

SwitchButtonWidget::SwitchButtonWidget(QWidget *parent)
    : QWidget(parent)
    , m_checked(false)
    , m_enabled(true)
    , m_pressed(false)
    , m_onColor(QColor(0, 200, 0))
    , m_offColor(QColor(150, 150, 150))
    , m_disabledColor(QColor(80, 80, 80))
{
    setMinimumSize(60, 30);
    setCursor(Qt::PointingHandCursor);
}

void SwitchButtonWidget::setChecked(bool checked)
{
    if (m_checked != checked) {
        m_checked = checked;
        update();
        emit stateChanged(m_checked);
    }
}

void SwitchButtonWidget::toggle()
{
    setChecked(!m_checked);
}

void SwitchButtonWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    drawBackground(painter);
    drawHandle(painter);
}

void SwitchButtonWidget::drawBackground(QPainter &painter)
{
    int h = height();
    int w = width();
    int radius = h / 2;
    QColor bgColor = !m_enabled ? m_disabledColor : (m_checked ? m_onColor : m_offColor);

    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawRoundedRect(0, 0, w, h, radius, radius);
}

void SwitchButtonWidget::drawHandle(QPainter &painter)
{
    int h = height();
    int w = width();
    int handleSize = h - 8;
    int handleX = m_checked ? w - handleSize - 4 : 4;

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(handleX, 4, handleSize, handleSize);
}

void SwitchButtonWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_enabled && event->button() == Qt::LeftButton)
        m_pressed = true;
    QWidget::mousePressEvent(event);
}

void SwitchButtonWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_enabled && m_pressed && event->button() == Qt::LeftButton) {
        m_pressed = false;
        toggle();
        emit clicked();
    }
    QWidget::mouseReleaseEvent(event);
}
