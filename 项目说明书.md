# 项目说明书

### 1、摘要

针对工业现场设备数据采集分散、远程监测困难、设备状态不易统一管理等问题，设计并实现了一套基于 MQTT 的工业物联网监测系统。系统采用 i.MX6ULL ARM 嵌入式 Linux 开发板作为现场数据采集端，通过 RS485 物理接口和 Modbus RTU 通信协议采集温湿度传感器、继电器等设备数据；PC 端作为上位机监控端，负责设备数据显示、远程管理和历史数据存储。系统嵌入式端和 PC 端均采用前后端分离架构，前端界面基于 Qt 实现，后端服务采用 C/C++ 开发，并通过 IPC 机制实现前端界面与后端服务之间的数据通信。系统利用 MQTT 协议完成板端与 PC 端的数据互通，使用 SQLite 数据库存储设备配置、实时数据和历史数据。系统主要实现了设备数据采集、远程数据显示、设备添加与删除、设备状态监测、历史数据存储等功能。实际运行表明，本系统能够较好地模拟工业物联网中边缘网关与上位机协同工作的应用场景，提高了设备数据采集与远程监测的效率，具有一定的实用价值和扩展能力。

### 2、通信结构

**1. Linux_data -> Pc_data，MQTT 上行包**

固定 topic 设计：

| 用途          | topic                             |
| ------------- | --------------------------------- |
| 注册类        | `gateway/register`                |
| 网关上行      | `gateway/{gatewayId}/up`          |
| 端口/设备上行 | `gateway/{gatewayId}/{portId}/up` |
| 网关命令      | `cmd/{gatewayId}`                 |
| 端口命令      | `cmd/{gatewayId}/{portId}`        |

主要包：

| `type`                        | 作用                                        |
| ----------------------------- | ------------------------------------------- |
| `gateway_register`            | 网关注册                                    |
| `gateway_heartbeat`           | 网关心跳                                    |
| `gateway_status`              | Pc_data 兼容处理的网关状态包                |
| `port_register`               | RS485 端口注册/连接状态                     |
| `port_status`                 | 端口状态兼容包                              |
| `device_register`             | 单个设备注册                                |
| `config_snapshot`             | 全量配置快照                                |
| `device_config_snapshot`      | 设备配置快照，和 `config_snapshot` 同类处理 |
| `telemetry_pack`              | 遥测数据包                                  |
| `alarm_event`                 | 新版报警事件                                |
| `command` + `cmd:"emergency"` | 旧版紧急报警兼容                            |
| `ack`                         | Linux_data 对命令/注册/快照的最终回执       |
| `command_ack`                 | Pc_data 兼容识别的命令回执                  |
| `command_result`              | Pc_data 兼容识别的命令结果                  |
| `threshold_config`            | 测点/阈值配置包                             |

核心字段规律：

```
{
  "type": "telemetry_pack",
  "version": 1,
  "sequence": 123,
  "seq": 123,
  "timestampMs": 1710000000000,
  "sourceId": "...",
  "targetId": "...",
  "site": {
    "factoryId": "factory_001",
    "areaId": "area_001",
    "gatewayId": "gateway_001",
    "portId": "port_001"
  },
  "devices": []
}
```

`devices[]` 里按设备类型变化：

| 设备类型         | 字段                                                         |
| ---------------- | ------------------------------------------------------------ |
| `sensor_th`      | `temperature`, `humidity`, `th`, `temp`, `humi`, `points[]`  |
| `relay`          | `relayStates`, `channelCount`, `states`, `relay`, `points[]` |
| `electric_meter` | `voltage`, `current`, `power`, `energy`, `meter`             |
| `sysinfo`        | `kernel`, `arch`, `os`, `screenWidth`, `screenHeight`, `cpuUsage`, `memoryUsage` |

**2. Pc_data -> Linux_data，MQTT 命令包**

顶层基本是：

```
{
  "type": "command",
  "cmd": "add_device",
  "cmd_id": "...",
  "seq": 123,
  "gatewayId": "gateway_001",
  "portId": "port_001",
  "timestampMs": 1710000000000,
  "payload": {}
}
```

Linux_data 当前支持的 `cmd` 有：

| `cmd`                      | 作用                            |
| -------------------------- | ------------------------------- |
| `scan_ports`               | 扫描串口                        |
| `get_runtime_state`        | 获取运行状态                    |
| `connect_port`             | 连接 RS485 端口                 |
| `disconnect_port`          | 断开 RS485 端口                 |
| `add_device`               | 添加设备                        |
| `remove_device`            | 删除设备                        |
| `set_relay`                | 控制继电器                      |
| `set_device_threshold`     | 设置设备阈值                    |
| `set_threshold`            | `set_device_threshold` 兼容别名 |
| `get_config`               | 请求配置快照                    |
| `request_config_snapshot`  | 请求配置快照                    |
| `get_offline_cache_config` | 获取离线缓存配置                |
| `set_offline_cache_config` | 设置离线缓存配置                |
| `clear_offline_cache`      | 清空离线缓存                    |
| `flush_offline_cache`      | 触发离线缓存发送                |

`add_device` 的关键字段：

```
{
  "type": "command",
  "cmd": "add_device",
  "seq": 123,
  "gatewayId": "gateway_001",
  "portId": "port_001",
  "slot": 0,
  "slave_id": 1,
  "deviceId": 1,
  "device_type": "sensor_th",
  "deviceType": "sensor_th",
  "poll_interval_ms": 1000,
  "pollIntervalMs": 1000,
  "device": {}
}
```

`remove_device` 关键字段：

```
{
  "type": "command",
  "cmd": "remove_device",
  "seq": 123,
  "gatewayId": "gateway_001",
  "portId": "port_001",
  "slot": 0,
  "slave_id": 1,
  "deviceId": 1
}
```

**3. Linux_data -> Linux_ui，本地 IPC 包**

这里是板端后端给嵌入式 Qt UI 的 IPC，不是 PC 端 MQTT。

| `type`                          | 作用                                                |
| ------------------------------- | --------------------------------------------------- |
| `telemetry_pack`                | 本地实时数据显示                                    |
| `ack`                           | 命令执行结果                                        |
| `publish_ack`                   | Linux_data 本地发布结果，表示 IPC/MQTT 发送是否成功 |
| `config_sync_state`             | 配置同步状态                                        |
| `device_register`               | 设备注册通知                                        |
| `command` + `cmd:"port_status"` | 端口状态兼容                                        |
| `command` + `cmd:"emergency"`   | 紧急报警兼容                                        |

注意：`publish_ack` 只是“发送层结果”，不是业务成功。业务成功要看 Linux_data 最终 `ack`：`stage:"done" && ok:true && status:"success"`。

**4. Pc_data -> Pc_ui，PC IPC 下发包**

Pc_data 会把 MQTT、数据库、运行状态整理成 UI 能直接用的 IPC 包：

| `type`                    | 作用                         |
| ------------------------- | ---------------------------- |
| `latest_points`           | 最新测点数据                 |
| `devices_snapshot`        | 设备树/设备快照              |
| `gateway_status_snapshot` | 网关状态快照                 |
| `port_status_snapshot`    | 端口状态快照                 |
| `command_ack`             | 命令状态回执，PC 侧统一给 UI |
| `command_log_update`      | 命令日志状态更新             |
| `history_points`          | 历史曲线/历史表格查询结果    |
| `delete_data_ack`         | 删除数据/清空数据结果        |
| `alarm_event`             | 单条报警事件推送             |
| `alarm_events_snapshot`   | 报警列表快照                 |
| `alarm_action_ack`        | 报警确认/清理动作结果        |
| `mqtt_config`             | MQTT 配置查询结果            |
| `mqtt_config_ack`         | MQTT 配置保存结果            |
| `sync_config_result`      | 同步配置结果                 |

**5. Pc_ui -> Pc_data，PC IPC 请求包**

| `type`                      | 作用                             |
| --------------------------- | -------------------------------- |
| `get_latest_points`         | 请求最新点位                     |
| `query_history`             | 查询历史数据                     |
| `get_alarm_events`          | 获取报警列表                     |
| `ack_alarm`                 | 确认单个报警                     |
| `clear_acknowledged_alarms` | 清理已确认报警                   |
| `clear_recovered_alarms`    | 清理已恢复报警                   |
| `clear_all_data`            | 清空运行数据                     |
| `delete_device_data`        | 删除某设备 PC 侧数据             |
| `delete_master_data`        | 删除某端口/主站 PC 侧数据        |
| `get_mqtt_config`           | 获取 MQTT 配置                   |
| `save_mqtt_config`          | 保存 MQTT 配置                   |
| `sync_config_request`       | 从网关同步配置                   |
| `command`                   | UI 发远程命令，Pc_data 再转 MQTT |

Pc_ui 远程命令允许的 `cmd/commandType`：

```
add_device
remove_device
set_relay
set_device_threshold
set_threshold
get_config
request_config_snapshot
discover
```

**6. 最容易写错的点**

`add_device`、`remove_device` 不是独立 JSON 包类型，而是：

```
{"type":"command","cmd":"add_device"}
{"type":"command","cmd":"remove_device"}
```

`alarm` 也不是当前主协议的正式 `type`，正式是：

```
{"type":"alarm_event"}
```

旧兼容才是：

```
{"type":"command","cmd":"emergency"}
```

配置类包都走 `gateway/register`，包括：

```
gateway_register
port_register
device_register
config_snapshot
device_config_snapshot
```

遥测必须走：

```
gateway/{gatewayId}/{portId}/up
```

命令必须走：

```
cmd/{gatewayId}
cmd/{gatewayId}/{portId}
```

### 3、数据回报Pc和Linux 回报划分和mqtt的messageType处理

```
Pc_data主要就是telemetry的解包
发送的包要在device和device_status存在才能接收数据否则就丢包不显示 但是可以加电警告显示丢弃的数据

```

| 顺序   | messageType                 | 说明             |
| ------ | --------------------------- | ---------------- |
| 1      | `alarm_event`               | 新版报警事件     |
| 1-兼容 | `command` + `cmd=emergency` | 旧版紧急报警兼容 |
| 2      | `gateway_register`          | 网关注册         |
| 3      | `gateway_heartbeat`         | 网关心跳         |
| 4      | `port_register`             | 端口注册         |
| 5      | `ack`                       | 命令回执         |
| 6      | `config_snapshot`           | 配置快照         |
| 7      | `device_register`           | 单设备注册       |
| 8      | `threshold_config`          | 阈值配置         |
| 9      | `telemetry_pack`            | 遥测数据包       |

规则

Pc_data



### 5、MQTT主题设置

结构

```
Python Mock / Linux_data
        │
        │  MQTT JSON 按行/单包消息
        ▼
EMQX Broker
127.0.0.1:1883
        │
        ▼
Pc_data
解析 MQTT → 存内存快照/数据库 → 通过 IPC 推送给 Pc_ui
        │
        ▼
Pc_ui
Windows Named Pipe: PcDataIpcPipe

```

主题设置

```
注册类入口        gateway/register
网关上行          gateway/{gatewayId}/up
端口上行          gateway/{gatewayId}/{portId}/up
网关命令          cmd/{gatewayId}
端口/设备命令     cmd/{gatewayId}/{portId}


所以的设备注册统一接口都是再gateway/register而且必须每一次都要有ack回包是通过seq确认是哪个包
通过 gateway/{gatewayId}/up和gateway/{gatewayId}/{portId}/up发送数据包和发送心跳
cmd 是负责统一的从Pc端发送命令和删除命令而且每次也要有ack回包

注意：
	如果我发送gateway/register注册失败或没有回包说明mqtt没有接收到或者没有开启就把Linux_data的注册放到data_config_sync进行注册缓存隔断时间进行数据处理
```

### 6、数据库设计

一、数据库表总览

| 序号 | 表名                | 中文名称       | 主要作用                                                     |
| ---- | ------------------- | -------------- | ------------------------------------------------------------ |
| 1    | `gateway_registry`  | 网关注册表     | 保存网关注册信息、通信 Topic、最后注册时间和心跳时间         |
| 2    | `gateway_status`    | 网关状态表     | 保存网关在线状态、所属工厂、所属区域等信息                   |
| 3    | `gateway_port`      | 网关端口表     | 保存网关下 RS485 端口信息，例如端口名称、串口路径、波特率、状态 |
| 4    | `command_log`       | 命令日志表     | 记录 Pc_ui 下发命令的执行过程和结果                          |
| 5    | `device`            | 设备表         | 保存从站设备配置，例如设备 ID、设备名称、设备类型、轮询周期  |
| 6    | `device_status`     | 设备状态表     | 保存设备在线、离线、最后出现时间等运行状态                   |
| 7    | `point_config`      | 点位配置表     | 保存设备点位信息，例如温度、湿度、继电器通道、报警阈值       |
| 8    | `latest_point`      | 实时点位数据表 | 保存每个点位的最新一次采集值                                 |
| 9    | `telemetry_history` | 历史遥测数据表 | 保存点位历史采集数据，用于趋势曲线和历史查询                 |
| 10   | `alarm_event`       | 报警事件表     | 保存设备点位报警事件、恢复状态和确认状态                     |

1. gateway_registry：网关注册表

| 字段名                   | 类型    | 约束        | 字段说明                 |
| ------------------------ | ------- | ----------- | ------------------------ |
| `gateway_id`             | TEXT    | PRIMARY KEY | 网关唯一标识             |
| `gateway_name`           | TEXT    |             | 网关名称                 |
| `status`                 | TEXT    |             | 网关注册状态             |
| `up_topic`               | TEXT    |             | 网关上行 Topic           |
| `cmd_topic`              | TEXT    |             | 网关命令 Topic           |
| `broadcast_topic`        | TEXT    |             | 广播 Topic               |
| `last_register_time_ms`  | INTEGER |             | 最后注册时间，毫秒时间戳 |
| `last_heartbeat_time_ms` | INTEGER |             | 最后心跳时间，毫秒时间戳 |
| `update_time_ms`         | INTEGER |             | 更新时间，毫秒时间戳     |

------

2. gateway_status：网关状态表

| 字段名                   | 类型    | 约束        | 字段说明                       |
| ------------------------ | ------- | ----------- | ------------------------------ |
| `gateway_id`             | TEXT    | PRIMARY KEY | 网关唯一标识                   |
| `gateway_name`           | TEXT    |             | 网关名称                       |
| `factory_id`             | TEXT    |             | 所属工厂 ID                    |
| `area_id`                | TEXT    |             | 所属区域 ID                    |
| `status`                 | TEXT    | NOT NULL    | 网关状态，例如 online、offline |
| `last_register_time_ms`  | INTEGER |             | 最后注册时间，毫秒时间戳       |
| `last_heartbeat_time_ms` | INTEGER |             | 最后心跳时间，毫秒时间戳       |
| `update_time_ms`         | INTEGER |             | 更新时间，毫秒时间戳           |

------

3. gateway_port：网关端口表

| 字段名                  | 类型    | 约束     | 字段说明                 |
| ----------------------- | ------- | -------- | ------------------------ |
| `gateway_id`            | TEXT    | NOT NULL | 所属网关 ID              |
| `port_id`               | TEXT    | NOT NULL | 端口 ID                  |
| `port_name`             | TEXT    |          | 端口名称，例如 RS485-1   |
| `slot`                  | INTEGER |          | 端口槽位编号             |
| `device_path`           | TEXT    |          | 串口设备路径             |
| `baud`                  | INTEGER |          | 波特率                   |
| `status`                | TEXT    | NOT NULL | 端口状态                 |
| `last_register_time_ms` | INTEGER |          | 最后注册时间，毫秒时间戳 |
| `update_time_ms`        | INTEGER |          | 更新时间，毫秒时间戳     |

主键：

| 主键字段     |
| ------------ |
| `gateway_id` |
| `port_id`    |

------

4. command_log：命令日志表

| 字段名           | 类型    | 约束        | 字段说明                                            |
| ---------------- | ------- | ----------- | --------------------------------------------------- |
| `command_id`     | TEXT    | PRIMARY KEY | 命令唯一 ID                                         |
| `seq`            | INTEGER | NOT NULL    | 命令序号                                            |
| `command_type`   | TEXT    | NOT NULL    | 命令类型，例如 add_device、remove_device、set_relay |
| `gateway_id`     | TEXT    |             | 目标网关 ID                                         |
| `port_id`        | TEXT    |             | 目标端口 ID                                         |
| `device_id`      | INTEGER |             | 目标设备 ID                                         |
| `status`         | TEXT    | NOT NULL    | 命令状态                                            |
| `reason`         | TEXT    |             | 失败原因                                            |
| `message`        | TEXT    |             | 命令提示信息                                        |
| `create_time_ms` | INTEGER |             | 命令创建时间，毫秒时间戳                            |
| `send_time_ms`   | INTEGER |             | 命令发送时间，毫秒时间戳                            |
| `finish_time_ms` | INTEGER |             | 命令完成时间，毫秒时间戳                            |

------

5. device：设备表

| 字段名             | 类型    | 约束                      | 字段说明                               |
| ------------------ | ------- | ------------------------- | -------------------------------------- |
| `id`               | INTEGER | PRIMARY KEY AUTOINCREMENT | 自增主键                               |
| `factory_id`       | TEXT    |                           | 工厂 ID                                |
| `factory_name`     | TEXT    |                           | 工厂名称                               |
| `area_id`          | TEXT    |                           | 区域 ID                                |
| `area_name`        | TEXT    |                           | 区域名称                               |
| `gateway_id`       | TEXT    | NOT NULL                  | 所属网关 ID                            |
| `gateway_name`     | TEXT    |                           | 网关名称                               |
| `port_id`          | TEXT    | NOT NULL                  | 所属端口 ID                            |
| `port_name`        | TEXT    |                           | 端口名称                               |
| `device_id`        | INTEGER | NOT NULL                  | 设备 ID，也就是从站地址                |
| `device_name`      | TEXT    |                           | 设备名称                               |
| `device_type`      | TEXT    | NOT NULL                  | 设备类型，例如 sensor_th、relay        |
| `poll_interval_ms` | INTEGER | DEFAULT 1000              | 轮询周期，单位毫秒                     |
| `expect_telemetry` | INTEGER | DEFAULT 1                 | 是否期望接收遥测数据                   |
| `enabled`          | INTEGER | DEFAULT 1                 | 是否启用                               |
| `deleted`          | INTEGER | DEFAULT 0                 | 是否已删除，0 表示未删除，1 表示已删除 |
| `deleted_time_ms`  | INTEGER | DEFAULT 0                 | 删除时间，毫秒时间戳                   |
| `status`           | TEXT    | DEFAULT 'active'          | 设备状态                               |
| `create_time_ms`   | INTEGER |                           | 创建时间，毫秒时间戳                   |
| `update_time_ms`   | INTEGER |                           | 更新时间，毫秒时间戳                   |

唯一约束：

| 唯一字段     |
| ------------ |
| `gateway_id` |
| `port_id`    |
| `device_id`  |

------

6. device_status：设备状态表

| 字段名            | 类型    | 约束                       | 字段说明                                |
| ----------------- | ------- | -------------------------- | --------------------------------------- |
| `id`              | INTEGER | PRIMARY KEY AUTOINCREMENT  | 自增主键                                |
| `gateway_id`      | TEXT    | NOT NULL                   | 所属网关 ID                             |
| `port_id`         | TEXT    | NOT NULL                   | 所属端口 ID                             |
| `device_id`       | INTEGER | NOT NULL                   | 设备 ID                                 |
| `status`          | TEXT    | NOT NULL DEFAULT 'unknown' | 设备状态，例如 online、offline、unknown |
| `last_seen_ms`    | INTEGER | DEFAULT 0                  | 最后在线时间，毫秒时间戳                |
| `last_offline_ms` | INTEGER | DEFAULT 0                  | 最后离线时间，毫秒时间戳                |
| `status_reason`   | TEXT    |                            | 状态原因                                |
| `update_time_ms`  | INTEGER |                            | 更新时间，毫秒时间戳                    |

唯一约束：

| 唯一字段     |
| ------------ |
| `gateway_id` |
| `port_id`    |
| `device_id`  |

------

7. point_config：点位配置表

| 字段名           | 类型    | 约束        | 字段说明                                    |
| ---------------- | ------- | ----------- | ------------------------------------------- |
| `point_id`       | TEXT    | PRIMARY KEY | 点位唯一 ID                                 |
| `factory_id`     | TEXT    |             | 工厂 ID                                     |
| `factory_name`   | TEXT    |             | 工厂名称                                    |
| `area_id`        | TEXT    |             | 区域 ID                                     |
| `area_name`      | TEXT    |             | 区域名称                                    |
| `gateway_id`     | TEXT    |             | 网关 ID                                     |
| `gateway_name`   | TEXT    |             | 网关名称                                    |
| `port_id`        | TEXT    |             | 端口 ID                                     |
| `port_name`      | TEXT    |             | 端口名称                                    |
| `device_id`      | INTEGER |             | 设备 ID                                     |
| `device_name`    | TEXT    |             | 设备名称                                    |
| `device_type`    | TEXT    |             | 设备类型                                    |
| `point_key`      | TEXT    | NOT NULL    | 点位键，例如 temperature、humidity、relay_1 |
| `point_name`     | TEXT    | NOT NULL    | 点位名称                                    |
| `unit`           | TEXT    |             | 单位，例如 ℃、%、开关状态                   |
| `value_type`     | TEXT    | NOT NULL    | 数据类型，例如 number、bool、text           |
| `enable_alarm`   | INTEGER | DEFAULT 0   | 是否启用报警                                |
| `alarm_low`      | REAL    |             | 报警下限                                    |
| `alarm_high`     | REAL    |             | 报警上限                                    |
| `enabled`        | INTEGER | DEFAULT 1   | 点位是否启用                                |
| `create_time_ms` | INTEGER |             | 创建时间，毫秒时间戳                        |
| `update_time_ms` | INTEGER |             | 更新时间，毫秒时间戳                        |

------

8. latest_point：实时点位数据表

| 字段名          | 类型    | 约束        | 字段说明                 |
| --------------- | ------- | ----------- | ------------------------ |
| `point_id`      | TEXT    | PRIMARY KEY | 点位唯一 ID              |
| `timestamp_ms`  | INTEGER | NOT NULL    | 数据采集时间，毫秒时间戳 |
| `factory_id`    | TEXT    |             | 工厂 ID                  |
| `factory_name`  | TEXT    |             | 工厂名称                 |
| `area_id`       | TEXT    |             | 区域 ID                  |
| `area_name`     | TEXT    |             | 区域名称                 |
| `gateway_id`    | TEXT    |             | 网关 ID                  |
| `gateway_name`  | TEXT    |             | 网关名称                 |
| `port_id`       | TEXT    |             | 端口 ID                  |
| `port_name`     | TEXT    |             | 端口名称                 |
| `device_id`     | INTEGER |             | 设备 ID                  |
| `device_name`   | TEXT    |             | 设备名称                 |
| `device_type`   | TEXT    |             | 设备类型                 |
| `point_key`     | TEXT    |             | 点位键                   |
| `point_name`    | TEXT    |             | 点位名称                 |
| `unit`          | TEXT    |             | 单位                     |
| `value_type`    | TEXT    |             | 数据类型                 |
| `number_value`  | REAL    |             | 数值型数据               |
| `text_value`    | TEXT    |             | 文本型数据               |
| `valid`         | INTEGER |             | 数据是否有效             |
| `error_message` | TEXT    |             | 错误信息                 |

------

9. telemetry_history：历史遥测数据表

| 字段名          | 类型    | 约束                      | 字段说明                 |
| --------------- | ------- | ------------------------- | ------------------------ |
| `id`            | INTEGER | PRIMARY KEY AUTOINCREMENT | 自增主键                 |
| `point_id`      | TEXT    | NOT NULL                  | 点位唯一 ID              |
| `timestamp_ms`  | INTEGER | NOT NULL                  | 数据采集时间，毫秒时间戳 |
| `factory_id`    | TEXT    |                           | 工厂 ID                  |
| `factory_name`  | TEXT    |                           | 工厂名称                 |
| `area_id`       | TEXT    |                           | 区域 ID                  |
| `area_name`     | TEXT    |                           | 区域名称                 |
| `gateway_id`    | TEXT    |                           | 网关 ID                  |
| `gateway_name`  | TEXT    |                           | 网关名称                 |
| `port_id`       | TEXT    |                           | 端口 ID                  |
| `port_name`     | TEXT    |                           | 端口名称                 |
| `device_id`     | INTEGER |                           | 设备 ID                  |
| `device_name`   | TEXT    |                           | 设备名称                 |
| `device_type`   | TEXT    |                           | 设备类型                 |
| `point_key`     | TEXT    |                           | 点位键                   |
| `point_name`    | TEXT    |                           | 点位名称                 |
| `unit`          | TEXT    |                           | 单位                     |
| `value_type`    | TEXT    |                           | 数据类型                 |
| `number_value`  | REAL    |                           | 数值型数据               |
| `text_value`    | TEXT    |                           | 文本型数据               |
| `valid`         | INTEGER |                           | 数据是否有效             |
| `error_message` | TEXT    |                           | 错误信息                 |

------

10. alarm_event：报警事件表

| 字段名            | 类型    | 约束                      | 字段说明                                       |
| ----------------- | ------- | ------------------------- | ---------------------------------------------- |
| `alarm_id`        | INTEGER | PRIMARY KEY AUTOINCREMENT | 报警事件自增主键                               |
| `point_id`        | TEXT    | NOT NULL                  | 触发报警的点位 ID                              |
| `factory_id`      | TEXT    |                           | 工厂 ID                                        |
| `factory_name`    | TEXT    |                           | 工厂名称                                       |
| `area_id`         | TEXT    |                           | 区域 ID                                        |
| `area_name`       | TEXT    |                           | 区域名称                                       |
| `gateway_id`      | TEXT    |                           | 网关 ID                                        |
| `gateway_name`    | TEXT    |                           | 网关名称                                       |
| `port_id`         | TEXT    |                           | 端口 ID                                        |
| `port_name`       | TEXT    |                           | 端口名称                                       |
| `device_id`       | INTEGER |                           | 设备 ID                                        |
| `device_name`     | TEXT    |                           | 设备名称                                       |
| `device_type`     | TEXT    |                           | 设备类型                                       |
| `point_key`       | TEXT    |                           | 点位键                                         |
| `point_name`      | TEXT    |                           | 点位名称                                       |
| `alarm_type`      | TEXT    | NOT NULL                  | 报警类型，例如 threshold_high、threshold_low   |
| `alarm_level`     | TEXT    | NOT NULL                  | 报警级别，例如 warning、critical               |
| `alarm_message`   | TEXT    |                           | 报警描述                                       |
| `start_time_ms`   | INTEGER | NOT NULL                  | 报警开始时间，毫秒时间戳                       |
| `timestamp_ms`    | INTEGER |                           | 报警事件时间，毫秒时间戳                       |
| `recover_time_ms` | INTEGER |                           | 报警恢复时间，毫秒时间戳                       |
| `ack_time_ms`     | INTEGER |                           | 报警确认时间，毫秒时间戳                       |
| `status`          | TEXT    | NOT NULL                  | 报警状态，例如 active、recovered、acknowledged |
| `state`           | TEXT    |                           | 报警事件状态，例如 active、recovered           |
| `acked`           | INTEGER | DEFAULT 0                 | 是否已确认，0 表示未确认，1 表示已确认         |
| `trigger_value`   | REAL    |                           | 触发报警时的值                                 |
| `value`           | REAL    |                           | 当前值或恢复值                                 |
| `threshold_value` | REAL    |                           | 报警阈值                                       |
| `error_message`   | TEXT    |                           | 错误信息                                       |

------

三、主要表关系

| 关系           | 说明                                                         |
| -------------- | ------------------------------------------------------------ |
| 网关与端口     | 一个网关可以包含多个端口，对应关系为 `gateway_id`            |
| 端口与设备     | 一个端口可以挂载多个从站设备，对应关系为 `gateway_id + port_id` |
| 设备与点位     | 一个设备可以包含多个点位，对应关系为 `gateway_id + port_id + device_id` |
| 点位与实时数据 | 一个点位在 `latest_point` 中保存一条最新数据，对应关系为 `point_id` |
| 点位与历史数据 | 一个点位在 `telemetry_history` 中可以保存多条历史数据，对应关系为 `point_id` |
| 点位与报警事件 | 一个点位可以产生多条报警记录，对应关系为 `point_id`          |

------

四、主要唯一标识规则

| 对象     | 唯一标识                           |
| -------- | ---------------------------------- |
| 网关     | `gateway_id`                       |
| 端口     | `gateway_id + port_id`             |
| 设备     | `gateway_id + port_id + device_id` |
| 点位     | `point_id`                         |
| 命令     | `command_id`                       |
| 报警事件 | `alarm_id`                         |

### 8、脚本数据

| slave_id | 设备类型建议 | 主要测试目的          | 行为规则                                                     |
| -------- | ------------ | --------------------- | ------------------------------------------------------------ |
| 1        | sensor_th    | 正常添加、正常遥测    | 添加成功，正常发送温湿度 telemetry                           |
| 2        | sensor_th    | 添加失败：设备无响应  | add_device 失败，`reason=device_no_response`                 |
| 3        | sensor_th    | 重复添加测试          | 第一次添加成功；同端口重复添加失败，`reason=device_exists`   |
| 4        | sensor_th    | 添加失败：端口异常    | add_device 失败，`reason=port_not_found`                     |
| 5        | sensor_th    | 删除延迟 ACK          | remove_device 成功，但延迟 3 秒 ACK                          |
| 6        | sensor_th    | 添加延迟/普通延迟测试 | 你笔记里写的是延迟成功，但当前 `BehaviorProfile.py` 没有写死 6 号规则，需要看 scenario JSON 或 handler |
| 7        | relay        | 强制继电器类型        | add_device 成功，但强制 `device_type=relay`                  |
| 8        | sensor_th    | 添加延迟 ACK          | add_device 成功，但延迟 3 秒 ACK，`reason=ok_after_3s`       |
| 9        | sensor_th    | 高温报警后恢复        | 先正常，10~20 秒高温，之后恢复，发 `threshold_high active/recovered` |
| 10       | sensor_th    | 持续高温报警          | 先正常，10 秒后持续高温，只发 `threshold_high active`        |
| 11       | sensor_th    | 低温报警后恢复        | 先正常，10~20 秒低温，之后恢复，发 `threshold_low active/recovered` |
| 12       | sensor_th    | 持续低温报警          | 先正常，10 秒后持续低温，只发 `threshold_low active`         |
| 13       | sensor_th    | 高低温综合报警        | 高温 active/recovered，然后低温 active/recovered             |

在 pc_ui 进入 **设备配置**：

### 9、规则设置

1. **架构规则**：Linux_data 或 Python Mock 通过 MQTT 把现场数据发到 EMQX，Pc_data 订阅解析后保存到内存快照和数据库，再通过 IPC 推送给 Pc_ui 显示。
2. **Pc_ui 职责规则**：Pc_ui 只负责界面显示、用户操作和快照刷新，不直接读取数据库、不直接连接 MQTT、不直接处理现场数据。
3. **Pc_data 职责规则**：Pc_data 是 PC 侧数据中心，负责 MQTT 收发、协议解析、数据库保存、状态快照维护、ACK 关联和 IPC 服务。
4. **Linux_data 职责规则**：Linux_data 是现场侧数据源，负责设备采集、端口管理、命令执行、阈值判断、报警上报和离线缓存。
5. **Topic 规则**：注册和配置类消息统一发到 `gateway/register`，上行状态和数据发到 `gateway/{gatewayId}/up` 或 `gateway/{gatewayId}/{portId}/up`，命令发到 `cmd/{gatewayId}` 或 `cmd/{gatewayId}/{portId}`。
6. **Telemetry 规则**：`telemetry_pack` 是高频实时数据，只用于刷新实时点位和保存历史数据，不要求业务 ACK。
7. **ACK 总规则**：除了 `telemetry_pack` 和 ACK 包本身，所有 MQTT 业务消息都必须由接收方回 ACK。
8. **ACK 防循环规则**：`ack`、`command_ack` 这类 ACK 包不能再被 ACK，避免形成 ACK 循环。
9. **Command 规则**：所有控制命令必须带 `cmd_id`，并通过 `cmd_id` 完成 Pc_ui、Pc_data、Linux_data 之间的端到端闭环。
10. **Command 成功规则**：MQTT publish 成功只代表消息发出，只有 Linux_data 返回 `stage=done && ok=true && status=success` 才代表命令业务执行成功。
11. **普通上报 ACK 规则**：普通上报消息不强制使用 `cmd_id`，用 `seq + type/ack_for + gatewayId + portId + deviceId` 关联对应 ACK。
12. **注册规则**：`gateway_register`、`port_register`、`device_register` 用于建立 Pc_data 的网关、端口和设备注册表，并且必须由 Pc_data 回 ACK。
13. **快照规则**：`config_snapshot` 或 `device_config_snapshot` 用于同步 Linux_data 当前完整配置，Pc_data 保存后刷新 UI 并回 ACK。
14. **设备创建规则**：Pc_data 不允许通过 `telemetry_pack` 自动创建设备，设备只能来自注册消息、配置快照或成功的 `add_device` 最终 ACK。
15. **设备删除规则**：设备删除成功后 Pc_data 要删除本地数据或标记 tombstone，已删除设备不能被 telemetry、普通注册或旧快照自动复活。
16. **心跳规则**：Linux_data 周期发送 `gateway_heartbeat`，Pc_data 根据心跳更新时间判断网关在线状态，并按最终规则回 ACK。
17. **离线规则**：如果 Pc_data 长时间收不到网关心跳或设备数据，就将对应网关、端口或设备标记为离线并推送给 Pc_ui。
18. **阈值规则**：阈值配置由 Pc_ui 发起、Pc_data 转发、Linux_data 保存和判断，Pc_data 不作为阈值判断源。
19. **报警规则**：超过高阈值、低于低阈值或恢复正常都由 Linux_data 判断并上报 `alarm_event`，Pc_data 保存报警并推送 Pc_ui。
20. **GPIO 联动规则**：后续阈值触发 GPIO 或板载设备联动时，只在 Linux_data 实现，不放到 Pc_ui 或 Pc_data。
21. **离线缓存规则**：Linux_data 在 MQTT 断开时必须缓存关键业务消息，重连后再按顺序推送到 Pc_data。
22. **离线缓存优先级规则**：command ACK、报警、注册、配置快照和状态变化必须缓存，普通 telemetry 可以按项目策略选择全量缓存或采样缓存。
23. **Python Mock 规则**：Python Mock 必须按 Linux_data 的最终 MQTT 协议模拟注册、心跳、telemetry、ACK、报警和命令执行结果。
24. **UI 状态规则**：Pc_ui 必须显示命令已发送、等待回包、执行成功、执行失败、执行超时、设备在线、设备离线和报警状态。
25. **最终边界规则**：Pc_ui 做显示，Pc_data 做数据中心，Linux_data 做现场控制和判断，三者之间只通过明确的 MQTT/IPC JSON 协议交互。

8、数据不统一

````
好，这个我用**最简单人话 + 类比 + 画“生活版图”**给你讲清楚，你一定能懂。

---

# 🧠 一、先忘掉代码，我先给你一个生活类比

你现在这个系统，其实像一个“工厂”。

---

## 🏭 你的系统 = 一个工厂调度中心

---

### 📦 三种数据 = 三个不同部门

```
┌──────────────┐
│ 设备档案部门   │ → devices_snapshot（设备长什么样）
└──────────────┘

┌──────────────┐
│ 实时监控部门   │ → latest_points（温度/湿度在变）
└──────────────┘

┌──────────────┐
│ 订单状态部门   │ → command_ack（成功/失败/删除）
└──────────────┘
```

---

# ⚠️ 二、问题来了（关键）

你现在做的是：

> ❗没有一个“总调度室”

---

# 🧨 三、你现在的真实情况（非常重要）

你现在系统是这样：

```
三个部门各自直接通知 UI
```

---

## 📢 变成这样：

```
设备部门 → UI
实时部门 → UI
状态部门 → UI
```

---

# 💥 四、这会发生什么？

我给你一个很直观的画面：

---

## 🎬 UI 的真实工作状态

UI 现在像一个人：

```
        👨 UI
       / |  \
      /  |   \
设备   实时   状态
```

---

### ❗问题来了：

每个部门都在喊：

* “我更新了！”
* “我也更新了！”
* “我也改了！”

---

# 💣 结果就是：

## UI开始混乱：

### 1️⃣ 设备刚画好

👉 实时数据来了 → 改一遍

### 2️⃣ 实时刚更新

👉 command_ack 又来 → 再改一遍

### 3️⃣ snapshot 又来了

👉 直接整张表重画

---

# 🧠 五、所以你看到的现象是：

| 现象      | 原因             |
| ------- | -------------- |
| UI不刷新   | 有更新但没统一入口      |
| 偶尔刷新    | signal 触发顺序不同  |
| 3号/7号消失 | snapshot覆盖了旧UI |
| 数据跳动    | 多个部门同时改UI      |

---

# 🧩 六、我帮你翻译这张图（最关键）

你刚才那张图，其实在表达一句话：

---

## ❗“你没有总指挥，只有三个部门各自说了算”

---

# 🧠 七、正确应该是什么样？

---

## ✔ 正确结构（重点）

```
            ┌──────────────┐
            │ MQTT / IPC   │
            └──────┬───────┘
                   ▼
        ┌────────────────────┐
        │   总调度室(State)   │  ← ⭐唯一入口
        │                    │
        │ 合并三种数据       │
        │ snapshot + realtime + ack
        └─────────┬──────────┘
                  ▼
        ┌────────────────────┐
        │   只发一个信号      │
        │   UI_REFRESH       │
        └─────────┬──────────┘
                  ▼
                👨 UI
```

---

# 🔥 八、核心差异（你必须记住）

## ❌ 现在：

```
三个部门 → 直接影响 UI
```

---

## ✔ 正确：

```
三个部门 → 先汇总 → 一个结果 → 再给 UI
```

---

# 🧠 九、再用一句更简单的解释

---

## ❌ 你现在：

> UI = 被三个老板同时指挥

---

## ✔ 正确：

> UI = 只听一个总经理（StateStore）

---

# 💥 十、为什么 Qt 会出问题（关键理解）

Qt View 很傻一件事：

> 👉 它不会自己“猜数据变了”

它只认：

* reset
* dataChanged
* insert/remove

---

你现在的问题就是：

> ❌ 你只在“喊”，但没给 Qt“标准更新指令”

---

# 🚀 十一、帮你压缩成最简单一句话

---

## 🧨 你现在的问题本质：

> ❗“数据来源有3个，但UI更新入口有3×N个”

---

## ✔ 正确应该：

> ✔ “数据来源可以很多，但UI入口必须只有1个”

---

# 👍 如果你还不懂，我可以再帮你做一件事：

我可以给你画一个：

### 👉 “你现在代码逐行对应到这张图的位置”

或者：

### 👉 “帮你把 DeviceManager/DataManager 直接改成正确结构”

只要你说一句：
👉 **“帮我对照代码解释这张图”**

````

8、

```
# Pc_data / Pc_ui 工业级稳定架构重构审计与方案

## Summary
结论先说死：当前系统是典型的 `push-driven failure architecture`。  
实时链路不是单一真相，而是 `MQTT / DB / IPC / UI StateStore / DeviceManager` 多处并行写状态，telemetry 又被双重门控，导致“有数据但 UI 不更新”“add_device 才刷新”“1~2 秒跳变”。

关键证据见：
- [Pc_data/MqttMessageHandler.cpp](S:/QT_object/IM6ULL_git/Pc_data/mqtt/MqttMessageHandler.cpp:2451)
- [Pc_data/service/PcDataService.cpp](S:/QT_object/IM6ULL_git/Pc_data/service/PcDataService.cpp:26)
- [Pc_ui/MainWindow.cpp](S:/QT_object/IM6ULL_git/Pc_ui/MainWindow.cpp:204)
- [Pc_ui/core/DataManager.cpp](S:/QT_object/IM6ULL_git/Pc_ui/core/DataManager.cpp:286)
- [Pc_ui/core/UiStateStore.cpp](S:/QT_object/IM6ULL_git/Pc_ui/core/UiStateStore.cpp:29)

## 架构问题清单
1. `telemetry` 被两层 gate 掉。  
   `Pc_data` 先按 `gateway/port/device/pointConfig` 过滤，再到 `Pc_ui` 侧按 `DeviceManager` 是否已有设备再次丢弃，合法数据也会死在链路中。

2. UI 依赖 push，不是 pull。  
   `MainWindow::requestLatestPoints()` 只在首次连接调用一次，运行期 `requestFullSnapshot()` 还是空实现；UI 没有稳定节拍，只等推送。

3. 多源状态写同一模型。  
   `DataManager`、`DeviceManager`、`UiStateStore`、命令 ACK 都在改同一个“设备状态”概念，且 `UiStateStore::rebuildStateMirror()` 每次都全量重建。

4. `latest_points` 是补丁，不是真相。  
   `Pc_data` 里它既受内存状态影响，又在空时回落查 DB；`Pc_ui` 又把它当主刷新来源，天然不稳。

5. IPC 是单通道、单 dirty 位、无队列的节流推送。  
   `latestPublisherLoop()` 只保留一个 dirty 状态，连接断开就直接 skip，丢帧是设计结果，不是偶发 bug。

6. DB 仍在实时服务平面里。  
   虽然 telemetry 落库被拆到 writer 线程，但 DB 仍参与启动补帧、快照、离线扫描、命令日志、删除/同步流程，慢库会拖慢整个实时服务面。

## 数据流时序图
### 现状
`MQTT -> Parser -> 规则门控 -> PcDataService 内存快照 -> 异步 DB queue -> latest_points dirty -> IPC push -> Pc_ui 收包 -> DataManager -> DeviceManager -> UiStateStore -> Model reset -> UI`

### add_device 为什么会刷新
`UI add_device -> Pc_data command ack 成功 -> Pc_data 主动发送 devices_snapshot / latest_points / status snapshot -> Pc_ui 设备树先落地 -> 后续 telemetry 才能通过 DeviceManager gate`

### telemetry 为什么不刷新
`MQTT telemetry 到达 -> 若 gateway/port/device/pointConfig 任一未注册就被丢 -> 即便进了 Pc_ui，也会被 DataManager 因 DeviceManager 缺设备再次丢弃`

## 根因总结
1. **没有唯一实时真相**，而是多个缓存和镜像同时写。  
2. **UI 刷新是 push 驱动且带门控**，不是稳定 pull。  
3. **telemetry 在两端都被注册态绑死**，所以“有数据”不等于“可显示”。

## 目标架构
### 最终稳定链路
`MQTT -> Parser -> StateStore(唯一实时真相) -> UI Pull(500ms/1s) -> DB Writer(异步批量) -> IPC Sender(节流)`

### StateStore 存什么
- `device`：gateway / port / device / point_config / tombstone / 生命周期
- `realtime`：最新点值、接收时间、有效性、dirty seq
- `status`：网关/端口/设备在线离线、服务在线、告警状态
- `command`：待确认命令、ACK 关联、超时状态

### UI Pull 怎么工作
- Dashboard / Monitor 默认 `1s`
- 实时趋势页可 `500ms`
- 只从 `Pc_data` 拉内存快照，不查 DB
- `latest_points` 改成补偿/恢复用途，不再是唯一刷新触发器

### DB 如何批量写
- `queue + merge + batch`
- `latest_point` 按 `pointId` 合并，保留最新值
- `telemetry_history` 走批量追加
- 固定一个 DB writer 线程
- 每 `200~500ms` 或 `N` 条刷一次事务
- DB 忙时只堆积 writer 队列，不回压实时解析线程

### IPC 如何节流
- 只发“状态快照/增量”，不发每条 telemetry
- 单周期只保留最新 snapshot seq
- IPC 断开时丢弃推送，但 `StateStore` 不丢
- UI 通过 pull 自愈，而不是赌推送到达

## 关键修复清单
1. `Pc_data` 必须新增 `StateStore`  
   因为现在 telemetry、device registry、status、command 分散在多个对象里，无法形成唯一真相。

2. `latest_points` 应降级  
   保留为启动补偿、重连恢复、调试通道，不再作为主刷新链路。

3. telemetry 热路径必须去 DB  
   热路径只能做 parse + state merge + dirty 标记；任何事务/查询/忙等都会把实时链路拖成抖动链路。

4. UI 必须改 pull  
   只有 pull 才能稳定 1Hz / 500ms，不受 push 丢帧、门控、IPC 背压影响。

5. `DeviceManager / UiStateStore` 必须拆职责  
   `DeviceManager` 只管配置态，`UiStateStore` 只管视图态，二者都不应再把 telemetry 当作唯一驱动源。

## 关键判断
1. 当前系统是否属于 `push-driven failure architecture`：**是**。  
2. 是否存在 DB blocking real-time pipeline：**是**，至少在系统层面存在，哪怕 telemetry 落库已异步化。  
3. UI 不刷新是否是设计问题而不是 Qt 问题：**是设计问题**。  
4. 是否必须引入 StateStore 才能解决问题：**是**。  
5. 当前方案是否可以工业级上线：**否**，原因是状态不唯一、刷新不稳定、门控过深、故障恢复不自洽。

## Test Plan
- 静态验证 telemetry 是否仍被双层 gate。
- 验证 UI 是否改为周期 pull，而不是只等 push。
- 验证 `latest_points` 不再驱动主界面刷新。
- 验证 DB 写入与 UI 刷新之间没有同步等待。
- 验证 add_device、重连、断流、补帧四条路径都能回到同一 StateStore。

## Assumptions
- 不改 MQTT topic。
- 不改协议字段。
- 不重构 UI 视觉结构。
- 只做架构层修复，保留现有业务语义。
- 本次为静态审计，没有运行、构建或实机验证。

```

### 问题

```
1. 先确认 MQTT 主题有没有问题
   ↓
2. 确认 Python Mock 是否真的收到 Pc_data 下发命令
   ↓
3. 发现 add_device / remove_device 需要靠 cmd_id / seq 匹配 ACK
   ↓
4. 修 Pc_data ↔ Python 的 ACK 闭环
   ↓
5. 再看 Pc_data 有没有收到 telemetry_pack
   ↓
6. Pc_data debug 显示 telemetry 确实进来了
   ↓
7. 继续看 Pc_data 是否写入 DB / latest_point / telemetry_history
   ↓
8. 又怀疑数据库读写慢、DB 在热路径导致 UI 延迟
   ↓
9. 做 memory-first、异步 DB writer、150ms latest_points 合并
   ↓
10. 但现象还在：只有 add_device 成功后 UI 才明显刷新
   ↓
11. 于是继续查 Pc_data → Pc_ui IPC
   ↓
12. 确认 latest_points 确实发到 Pc_ui，Pc_ui 也收到
   ↓
13. Pc_ui debug 显示 DataManager 收到 latest_points 并 upsertRealtimeData
   ↓
14. 但表格/设备树读的是 DeviceManager 或被 devices_snapshot 覆盖
   ↓
15. 最后定位到：不是 DB 读写主因，而是 UI 数据源混乱 + snapshot 覆盖 realtime
```

10、command请求



