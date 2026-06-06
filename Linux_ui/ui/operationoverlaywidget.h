#pragma once

#include <QColor>
#include <QString>
#include <QTimer>
#include <QWidget>

class QPainter;

class OperationOverlayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit OperationOverlayWidget(QWidget *parent = nullptr);

    void showLoading(const QString &text);
    void showSuccess(const QString &text);
    void showFailure(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    enum State {
        Loading,
        Success,
        Failure
    };

    void showResult(State state, const QString &text);
    void drawSpinner(QPainter &painter, const QPointF &center);
    void drawResultIcon(QPainter &painter, const QPointF &center);

    QTimer *m_timer;
    QString m_text;
    State m_state;
    int m_frame;
    int m_generation;
};
