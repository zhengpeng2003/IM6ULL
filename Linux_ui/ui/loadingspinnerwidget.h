#pragma once

#include <QColor>
#include <QTimer>
#include <QWidget>

class LoadingSpinnerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LoadingSpinnerWidget(QWidget *parent = nullptr);

    void start();
    void stop();
    bool isRunning() const;

    void setSegmentCount(int count);
    void setInterval(int msec);
    void setColors(const QColor &activeColor, const QColor &inactiveColor);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QTimer *m_timer;
    int m_frame;
    int m_segmentCount;
    QColor m_activeColor;
    QColor m_inactiveColor;
};
