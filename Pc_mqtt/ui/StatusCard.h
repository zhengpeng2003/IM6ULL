#pragma once
#include <QWidget>
class QLabel;

class StatusCard : public QWidget
{
    Q_OBJECT
public:
    explicit StatusCard(const QString &title, QWidget *parent = nullptr);
    void setValue(const QString &value);
    void setSubText(const QString &text);

private:
    QLabel *m_title = nullptr;
    QLabel *m_value = nullptr;
    QLabel *m_subText = nullptr;
};
