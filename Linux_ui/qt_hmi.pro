QT += core gui network
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

SOURCES += \
    data/data_parser.cpp \
    main.cpp \
    pageui/addslavedialog.cpp \
    pageui/slavedetaildialog.cpp \
    pageui/slavelistdialog.cpp \
    pageui/TrendChartWidget.cpp \
    pages/pageinfo.cpp \
    pages/pagesetting.cpp \
    sensorui/devicedetailcardbaseui.cpp \
    sensorui/relaycontroldialog.cpp \
    sensorui/relaydetailcardui.cpp \
    sensorui/sensorthdetailcardui.cpp \
    ui/loadingspinnerwidget.cpp \
    ui/operationoverlaywidget.cpp \
    pages/pagetrend.cpp \
    ui/switchbuttonwidget.cpp \
    ui/widget.cpp \
    ui/TopStatusBar.cpp \
    ui/BottomNavBar.cpp \
    pages/PageStatus.cpp \
    ipc/ipc_client.cpp

HEADERS += \
    data/data_parser.h \
    pageui/addslavedialog.h \
    pageui/slavedetaildialog.h \
    pageui/slavelistdialog.h \
    pageui/TrendChartWidget.h \
    pages/pageinfo.h \
    pages/pagesetting.h \
    sensorui/devicedetailcardbaseui.h \
    sensorui/relaycontroldialog.h \
    sensorui/relaydetailcardui.h \
    sensorui/sensorthdetailcardui.h \
    ui/loadingspinnerwidget.h \
    ui/operationoverlaywidget.h \
    pages/pagetrend.h \
    ui/switchbuttonwidget.h \
    ui/widget.h \
    ui/TopStatusBar.h \
    ui/BottomNavBar.h \
    pages/PageStatus.h \
    ipc/ipc_client.h \
    data/data_protocol.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += res.qrc
