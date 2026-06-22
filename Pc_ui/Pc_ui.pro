QT += core gui widgets charts network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = Pc_ui
TEMPLATE = app

SOURCES += \
    ipc/ipcclient.cpp \
    main.cpp \
    MainWindow.cpp \
    sensorui/DeviceDetailCardBaseUi.cpp \
    sensorui/RelayDetailCardUi.cpp \
    sensorui/SensorThDetailCardUi.cpp \
    ui/TopBar.cpp \
    ui/SideBar.cpp \
    ui/CommandTaskPanel.cpp \
    ui/StatusCard.cpp \
    ui/DeviceTreeWidget.cpp \
    ui/AlarmTableWidget.cpp \
    ui/DeviceTypeConfigRegistry.cpp \
    pages/DashboardPage.cpp \
    pages/MonitorPage.cpp \
    pages/TrendPage.cpp \
    pages/DeviceConfigPage.cpp \
    pages/AlarmLogPage.cpp \
    pages/SystemSettingPage.cpp \
    core/DeviceManager.cpp \
    core/AlarmManager.cpp \
    core/DataManager.cpp \
    core/CommandManager.cpp \
    core/ConfigManager.cpp \
    core/UiStateStore.cpp \
    model/DeviceStateListModel.cpp

HEADERS += \
    MainWindow.h \
    ipc/ipcclient.h \
    sensorui/DeviceDetailCardBaseUi.h \
    sensorui/RelayDetailCardUi.h \
    sensorui/SensorThDetailCardUi.h \
    ui/TopBar.h \
    ui/SideBar.h \
    ui/CommandTaskPanel.h \
    ui/StatusCard.h \
    ui/DeviceTreeWidget.h \
    ui/AlarmTableWidget.h \
    ui/DeviceTypeConfigRegistry.h \
    pages/DashboardPage.h \
    pages/MonitorPage.h \
    pages/TrendPage.h \
    pages/DeviceConfigPage.h \
    pages/AlarmLogPage.h \
    pages/SystemSettingPage.h \
    core/DeviceManager.h \
    core/AlarmManager.h \
    core/DataManager.h \
    core/CommandManager.h \
    core/ConfigManager.h \
    core/UiStateStore.h \
    model/DeviceModel.h \
    model/TelemetryModel.h \
    model/DeviceStateListModel.h \
    model/AlarmModel.h \
    model/CommandModel.h \
    model/ConfigModel.h

RESOURCES += \
    res.qrc
