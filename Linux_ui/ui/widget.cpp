#include "widget.h"
#include <QVBoxLayout>

IpcClient *Widget::_Myclient = nullptr;   // ✅ static 定义只能在 cpp

Widget::Widget(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("MainWidget");
    setFixedSize(480, 272);
    initUI();

}

Widget::~Widget()
{
}

void Widget::initUI()
{
    _Myclient = new IpcClient(this);
    _Myclient->connectToServer("/tmp/device_ipc.sock");

    TopStatusBar *top = new TopStatusBar(this);
    BottomNavBar *bottom = new BottomNavBar(this);

    connect(bottom, &BottomNavBar::sigPageChanged,
            this, &Widget::slotChangePage);

    QStackedWidget *stack = new QStackedWidget(this);

    PageStatus  *pageStatus  = new PageStatus(this);
    PageSetting *pageSetting = new PageSetting(this);
    PageTrend   *pageTrend   = new PageTrend(this);
    Pageinfo    *pageInfo    = new Pageinfo(this);

    stack->addWidget(pageStatus);
    stack->addWidget(pageTrend);
    stack->addWidget(pageSetting);
    stack->addWidget(pageInfo);

    connect(_Myclient,&IpcClient::devicetrend,pageTrend,&PageTrend::addData);
    connect(_Myclient,&IpcClient::deviceinfo,pageInfo,&Pageinfo::addInfo);
    //connect(_Myclient,&IpcClient::devicesetting,pageSetting,&PageSetting::addSetting);重复连接
    stack->setCurrentIndex(0);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(top);
    layout->addWidget(stack, 1);
    layout->addWidget(bottom);

    m_stack = stack;
}

void Widget::slotChangePage(int index)
{
    m_stack->setCurrentIndex(index);
}
