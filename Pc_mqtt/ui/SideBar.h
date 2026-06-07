#pragma once
#include <QWidget>
#include <QVector>

class QPushButton;

class SideBar : public QWidget
{
    Q_OBJECT
public:
    explicit SideBar(QWidget *parent = nullptr);

signals:
    void pageChanged(int index);

private:
    QPushButton *createButton(const QString &text, int index);
    QVector<QPushButton*> m_buttons;
};
