QT += core gui widgets charts network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

TARGET = Pc_ui
TEMPLATE = app

SOURCES += \
    ipc/ipcclient.cpp \
    main.cpp \
    MainWindow.cpp \
    ui/TopBar.cpp \
    ui/SideBar.cpp \
    ui/StatusCard.cpp \
    ui/DeviceTreeWidget.cpp \
    ui/AlarmTableWidget.cpp \
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
    dialogs/AddAreaDialog.cpp \
    dialogs/AddGatewayDialog.cpp \
    dialogs/EditMasterDialog.cpp \
    dialogs/EditSlaveDialog.cpp \
    dialogs/AlarmDetailDialog.cpp \
    dialogs/ControlConfirmDialog.cpp

HEADERS += \
    MainWindow.h \
    ipc/ipcclient.h \
    ui/TopBar.h \
    ui/SideBar.h \
    ui/StatusCard.h \
    ui/DeviceTreeWidget.h \
    ui/AlarmTableWidget.h \
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
    model/DeviceModel.h \
    model/TelemetryModel.h \
    model/AlarmModel.h \
    model/CommandModel.h \
    model/ConfigModel.h \
    dialogs/AddAreaDialog.h \
    dialogs/AddGatewayDialog.h \
    dialogs/EditMasterDialog.h \
    dialogs/EditSlaveDialog.h \
    dialogs/AlarmDetailDialog.h \
    dialogs/ControlConfirmDialog.h

RESOURCES += resources/res.qrc
