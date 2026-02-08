#pragma once
#include <QPushButton>
#include <QPixmap>
class MyBtn : public QPushButton
{
    Q_OBJECT
public:
    explicit MyBtn(const QString& offImg,
                   const QString& onImg,
                   QWidget* parent = nullptr);

    void setState(bool on); // 外部更新状态
    void updateIcon(bool on);
public slots:
    void onClicked();
signals:
    void gpioClicked(bool targetState);

private:
    QPixmap m_offPixmap;
    QPixmap m_onPixmap;
    bool m_currentState = false;
};
