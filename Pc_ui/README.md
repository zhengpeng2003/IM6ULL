# Pc_mqtt 工业物联网 PC 上位机

这是一个 Qt Widgets 工业物联网监控平台骨架项目。

## 当前包含

- MainWindow + TopBar + SideBar + QStackedWidget
- Dashboard 首页总览
- Monitor 实时监控
- Trend 趋势分析
- DeviceConfig 设备配置
- AlarmLog 报警日志
- SystemSetting 系统设置
- DataManager
- DeviceManager
- AlarmManager
- CommandManager
- ConfigManager
- SQLite 初始化
- 白色工业后台风格 QSS

## 编译要求

- Qt 6.x
- Qt Widgets
- Qt Sql
- Qt Charts
- CMake 3.16+

## 下一步

1. 先运行假数据 UI。
2. 再接 Pc_data IPC 数据。
3. 再接板端 i.MX6ULL。
4. 再补齐配置页面、报警确认、趋势查询和 CSV 导出。
