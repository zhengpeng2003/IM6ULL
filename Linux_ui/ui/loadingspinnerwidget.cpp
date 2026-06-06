#include "loadingspinnerwidget.h"

#include <QPainter>
#include <QtMath>

LoadingSpinnerWidget::LoadingSpinnerWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_frame(0)
    , m_segmentCount(12)
    , m_activeColor(QColor(30, 34, 38))
    , m_inactiveColor(QColor(170, 176, 182))
{
    setFixedSize(18, 18);
    setVisible(false);

    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_frame = (m_frame + 1) % m_segmentCount;
        update();
    });
    m_timer->setInterval(80);
}

void LoadingSpinnerWidget::start()
{
    if (!m_timer->isActive())
        m_timer->start();

    show();
    update();
}

void LoadingSpinnerWidget::stop()
{
    m_timer->stop();
    hide();
}

bool LoadingSpinnerWidget::isRunning() const
{
    return m_timer->isActive();
}

void LoadingSpinnerWidget::setSegmentCount(int count)
{
    if (count < 6)
        count = 6;

    m_segmentCount = count;
    m_frame %= m_segmentCount;
    update();
}

void LoadingSpinnerWidget::setInterval(int msec)
{
    m_timer->setInterval(msec < 16 ? 16 : msec);
}

void LoadingSpinnerWidget::setColors(const QColor &activeColor, const QColor &inactiveColor)
{
    m_activeColor = activeColor;
    m_inactiveColor = inactiveColor;
    update();
}

void LoadingSpinnerWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    const int side = qMin(width(), height());
    const qreal outerRadius = side / 2.0;
    const qreal segmentW = qMax<qreal>(2.0, side * 0.14);
    const qreal segmentH = qMax<qreal>(4.0, side * 0.28);
    const qreal center = side / 2.0;

    painter.translate(width() / 2.0, height() / 2.0);

    for (int i = 0; i < m_segmentCount; ++i) {
        const int distance = (i - m_frame + m_segmentCount) % m_segmentCount;
        const qreal fade = 1.0 - (distance / static_cast<qreal>(m_segmentCount));
        QColor color = m_inactiveColor;

        color.setRedF(m_inactiveColor.redF() +
                      (m_activeColor.redF() - m_inactiveColor.redF()) * fade);
        color.setGreenF(m_inactiveColor.greenF() +
                        (m_activeColor.greenF() - m_inactiveColor.greenF()) * fade);
        color.setBlueF(m_inactiveColor.blueF() +
                       (m_activeColor.blueF() - m_inactiveColor.blueF()) * fade);
        color.setAlphaF(0.35 + 0.65 * fade);

        painter.save();
        painter.rotate(i * (360.0 / m_segmentCount));
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(-segmentW / 2.0,
                                       -outerRadius + center * 0.12,
                                       segmentW,
                                       segmentH),
                                segmentW / 2.0,
                                segmentW / 2.0);
        painter.restore();
    }
}
