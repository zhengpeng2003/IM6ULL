#include <QApplication>
#include <QFile>
#include <QDebug>
#include "ui/widget.h"
static void loadQss(QApplication &app)
{
    QFile file(":/style/style/industrial.qss");   // 当前工程相对路径
    if (!file.open(QFile::ReadOnly)) {
        qDebug() << "load qss failed";
        return;
    }
    app.setStyleSheet(file.readAll());
    file.close();
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    loadQss(a);    // ⭐ QSS 在这里真正生效

    Widget w;
    w.show();

    return a.exec();
}
