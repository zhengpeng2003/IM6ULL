QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    data/data_parser.cpp \
    main.cpp \
    pages/addslavedialog.cpp \
    pages/slavedetaildialog.cpp \
    pages/slavelistdialog.cpp \
    pages/TrendChartWidget.cpp \
    pages/pageinfo.cpp \
    pages/pagesetting.cpp \
    ui/loadingspinnerwidget.cpp \
    ui/operationoverlaywidget.cpp \
    pages/pagetrend.cpp \
    ui/switchbuttonwidget.cpp \
    ui/widget.cpp \
    ui/TopStatusBar.cpp \
    ui/BottomNavBar.cpp \
    sensorui/sensorui.cpp \
    pages/PageStatus.cpp \
    ipc/ipc_client.cpp

HEADERS += \
    data/data_parser.h \
    pages/addslavedialog.h \
    pages/slavedetaildialog.h \
    pages/slavelistdialog.h \
    pages/TrendChartWidget.h \
    pages/pageinfo.h \
    pages/pagesetting.h \
    ui/loadingspinnerwidget.h \
    ui/operationoverlaywidget.h \
    pages/pagetrend.h \
    ui/switchbuttonwidget.h \
    ui/widget.h \
    ui/TopStatusBar.h \
    ui/BottomNavBar.h \
    sensorui/sensorui.h \
    pages/PageStatus.h \
    ipc/ipc_client.h \
    data/data_protocol.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += res.qrc
