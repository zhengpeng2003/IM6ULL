#pragma once
#include <QWidget>
#include <QColor>
#include <QMouseEvent>
#include <QPainter>

class SwitchButtonWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SwitchButtonWidget(QWidget *parent = nullptr);
    void setChecked(bool checked);
    bool isChecked() const { return m_checked; }
    void toggle();

signals:
    void stateChanged(bool state);
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void drawBackground(QPainter &painter);
    void drawHandle(QPainter &painter);

private:
    bool m_checked;
    bool m_enabled;
    bool m_pressed;
    QColor m_onColor;
    QColor m_offColor;
    QColor m_disabledColor;
};
