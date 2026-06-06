#include "operationoverlaywidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QTimer>

OperationOverlayWidget::OperationOverlayWidget(QWidget *parent)
    : QWidget(parent)
    , m_timer(new QTimer(this))
    , m_state(Loading)
    , m_frame(0)
    , m_generation(0)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground, true);
    hide();

    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_frame = (m_frame + 1) % 12;
        update();
    });
    m_timer->setInterval(70);
}

void OperationOverlayWidget::showLoading(const QString &text)
{
    ++m_generation;
    m_text = text;
    m_state = Loading;
    m_frame = 0;
    m_timer->start();
    show();
    raise();
    update();
}

void OperationOverlayWidget::showSuccess(const QString &text)
{
    showResult(Success, text);
}

void OperationOverlayWidget::showFailure(const QString &text)
{
    showResult(Failure, text);
}

void OperationOverlayWidget::showResult(State state, const QString &text)
{
    ++m_generation;
    const int generation = m_generation;
    m_text = text;
    m_state = state;
    m_timer->stop();
    show();
    raise();
    update();

    QTimer::singleShot(1100, this, [this, generation]() {
        if (generation == m_generation)
            hide();
    });
}

void OperationOverlayWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), QColor(0, 0, 0, 96));

    const QSize panelSize(226, 128);
    const QRect panelRect((width() - panelSize.width()) / 2,
                          (height() - panelSize.height()) / 2,
                          panelSize.width(),
                          panelSize.height());

    painter.setPen(QPen(QColor(70, 78, 84), 1));
    painter.setBrush(QColor(36, 39, 42, 242));
    painter.drawRoundedRect(panelRect, 8, 8);

    const QPointF iconCenter(panelRect.center().x(), panelRect.top() + 45);
    if (m_state == Loading)
        drawSpinner(painter, iconCenter);
    else
        drawResultIcon(painter, iconCenter);

    painter.setPen(QColor(236, 240, 242));
    QFont textFont = painter.font();
    textFont.setPixelSize(15);
    textFont.setBold(true);
    painter.setFont(textFont);
    painter.drawText(QRect(panelRect.left() + 16,
                           panelRect.top() + 82,
                           panelRect.width() - 32,
                           28),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     m_text);
}

void OperationOverlayWidget::drawSpinner(QPainter &painter, const QPointF &center)
{
    painter.save();
    painter.translate(center);
    painter.setPen(Qt::NoPen);

    for (int i = 0; i < 12; ++i) {
        const int distance = (i - m_frame + 12) % 12;
        const qreal fade = 1.0 - distance / 12.0;
        QColor color(62, 190, 176);
        color.setAlphaF(0.18 + fade * 0.72);

        painter.save();
        painter.rotate(i * 30.0);
        painter.setBrush(color);
        painter.drawRoundedRect(QRectF(-3.0, -29.0, 6.0, 15.0), 3.0, 3.0);
        painter.restore();
    }

    painter.restore();
}

void OperationOverlayWidget::drawResultIcon(QPainter &painter, const QPointF &center)
{
    const QRectF iconRect(center.x() - 24, center.y() - 24, 48, 48);
    const bool ok = (m_state == Success);
    const QColor fill = ok ? QColor(30, 164, 149) : QColor(205, 91, 74);

    painter.setPen(Qt::NoPen);
    painter.setBrush(fill);
    painter.drawRoundedRect(iconRect, 10, 10);

    QPen pen(QColor(255, 255, 255), 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    if (ok) {
        QPainterPath path;
        path.moveTo(center.x() - 14, center.y() + 1);
        path.lineTo(center.x() - 4, center.y() + 11);
        path.lineTo(center.x() + 15, center.y() - 12);
        painter.drawPath(path);
    } else {
        painter.drawLine(QPointF(center.x() - 12, center.y() - 12),
                         QPointF(center.x() + 12, center.y() + 12));
        painter.drawLine(QPointF(center.x() + 12, center.y() - 12),
                         QPointF(center.x() - 12, center.y() + 12));
    }
}
