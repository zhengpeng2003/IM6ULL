#include "MainWindow.h"
#include <QApplication>
#include <QFile>

static void loadStyle(QApplication &app)
{
    QFile file(":/css/style/industrial_white.qss");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(file.readAll()));
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    loadStyle(app);

    MainWindow w;
    w.resize(1366, 768);
    w.show();

    return app.exec();
}
