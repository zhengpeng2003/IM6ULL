#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QStackedWidget>
#include "ui/TopStatusBar.h"
#include "ui/BottomNavBar.h"
#include "pages/pagesetting.h"
#include "pages/pagetrend.h"
#include "pages/PageStatus.h"
#include "pages/pageinfo.h"
#include "ipc/ipc_client.h"

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT
public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget();

public slots:
    void slotChangePage(int index);

public:
    static IpcClient * _Myclient;

private:
    void initUI();
    QStackedWidget *m_stack;
    Ui::Widget *ui;
};

#endif // WIDGET_H
