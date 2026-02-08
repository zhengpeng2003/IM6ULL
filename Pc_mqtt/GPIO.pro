QT       += core gui network mqtt

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    GpioWidget.cpp \
    MqttWidget.cpp \
    main.cpp \
    mainwidget.cpp \
    mybtn.cpp \
    tempinfowidget.cpp

HEADERS += \
    GpioWidget.h \
    MqttWidget.h \
    mainwidget.h \
    mybtn.h \
    tempinfowidget.h

FORMS += \
    GpioWidget.ui \
    MqttWidget.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
