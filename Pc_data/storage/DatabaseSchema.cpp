#include "DatabaseSchema.hpp"

std::vector<std::string> DatabaseSchema::tableSqlList()
{
    return {
        R"SQL(
        CREATE TABLE IF NOT EXISTS gateway_registry (
            gateway_id TEXT PRIMARY KEY,
            gateway_name TEXT,
            status TEXT,
            up_topic TEXT,
            cmd_topic TEXT,
            broadcast_topic TEXT,
            last_register_time_ms INTEGER,
            last_heartbeat_time_ms INTEGER,
            update_time_ms INTEGER
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS gateway_status (
            gateway_id TEXT PRIMARY KEY,
            gateway_name TEXT,
            factory_id TEXT,
            area_id TEXT,
            status TEXT NOT NULL,
            last_register_time_ms INTEGER,
            last_heartbeat_time_ms INTEGER,
            update_time_ms INTEGER
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS gateway_port (
            gateway_id TEXT NOT NULL,
            port_id TEXT NOT NULL,
            port_name TEXT,
            slot INTEGER,
            device_path TEXT,
            baud INTEGER,
            status TEXT NOT NULL,
            last_register_time_ms INTEGER,
            update_time_ms INTEGER,
            PRIMARY KEY(gateway_id, port_id)
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS command_log (
            command_id TEXT PRIMARY KEY,
            seq INTEGER NOT NULL,
            command_type TEXT NOT NULL,
            gateway_id TEXT,
            port_id TEXT,
            device_id INTEGER,
            status TEXT NOT NULL,
            reason TEXT,
            message TEXT,
            create_time_ms INTEGER,
            send_time_ms INTEGER,
            finish_time_ms INTEGER
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS device (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            factory_id TEXT,
            factory_name TEXT,
            area_id TEXT,
            area_name TEXT,
            gateway_id TEXT NOT NULL,
            gateway_name TEXT,
            port_id TEXT NOT NULL,
            port_name TEXT,

            device_id INTEGER NOT NULL,
            device_name TEXT,
            device_type TEXT NOT NULL,
            poll_interval_ms INTEGER DEFAULT 1000,
            expect_telemetry INTEGER DEFAULT 1,
            enabled INTEGER DEFAULT 1,
            deleted INTEGER DEFAULT 0,
            deleted_time_ms INTEGER DEFAULT 0,
            status TEXT DEFAULT 'active',

            create_time_ms INTEGER,
            update_time_ms INTEGER,

            UNIQUE(gateway_id, port_id, device_id)
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS device_status (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            gateway_id TEXT NOT NULL,
            port_id TEXT NOT NULL,
            device_id INTEGER NOT NULL,

            status TEXT NOT NULL DEFAULT 'unknown',
            last_seen_ms INTEGER DEFAULT 0,
            last_offline_ms INTEGER DEFAULT 0,
            status_reason TEXT,
            update_time_ms INTEGER,

            UNIQUE(gateway_id, port_id, device_id)
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS point_config (
            point_id TEXT PRIMARY KEY,

            factory_id TEXT,
            factory_name TEXT,
            area_id TEXT,
            area_name TEXT,
            gateway_id TEXT,
            gateway_name TEXT,
            port_id TEXT,
            port_name TEXT,

            device_id INTEGER,
            device_name TEXT,
            device_type TEXT,

            point_key TEXT NOT NULL,
            point_name TEXT NOT NULL,
            unit TEXT,
            value_type TEXT NOT NULL,

            enable_alarm INTEGER DEFAULT 0,
            alarm_low REAL,
            alarm_high REAL,

            enabled INTEGER DEFAULT 1,

            create_time_ms INTEGER,
            update_time_ms INTEGER
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS latest_point (
            point_id TEXT PRIMARY KEY,

            timestamp_ms INTEGER NOT NULL,

            factory_id TEXT,
            factory_name TEXT,
            area_id TEXT,
            area_name TEXT,
            gateway_id TEXT,
            gateway_name TEXT,
            port_id TEXT,
            port_name TEXT,

            device_id INTEGER,
            device_name TEXT,
            device_type TEXT,

            point_key TEXT,
            point_name TEXT,
            unit TEXT,
            value_type TEXT,

            number_value REAL,
            text_value TEXT,

            valid INTEGER,
            error_message TEXT
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS telemetry_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,

            point_id TEXT NOT NULL,
            timestamp_ms INTEGER NOT NULL,

            factory_id TEXT,
            factory_name TEXT,
            area_id TEXT,
            area_name TEXT,
            gateway_id TEXT,
            gateway_name TEXT,
            port_id TEXT,
            port_name TEXT,

            device_id INTEGER,
            device_name TEXT,
            device_type TEXT,

            point_key TEXT,
            point_name TEXT,
            unit TEXT,
            value_type TEXT,

            number_value REAL,
            text_value TEXT,

            valid INTEGER,
            error_message TEXT
        );
        )SQL",

        R"SQL(
        CREATE TABLE IF NOT EXISTS alarm_event (
            alarm_id INTEGER PRIMARY KEY AUTOINCREMENT,

            point_id TEXT NOT NULL,

            factory_id TEXT,
            factory_name TEXT,
            area_id TEXT,
            area_name TEXT,
            gateway_id TEXT,
            gateway_name TEXT,
            port_id TEXT,
            port_name TEXT,

            device_id INTEGER,
            device_name TEXT,
            device_type TEXT,

            point_key TEXT,
            point_name TEXT,

            alarm_type TEXT NOT NULL,
            alarm_level TEXT NOT NULL,
            alarm_message TEXT,

            start_time_ms INTEGER NOT NULL,
            timestamp_ms INTEGER,
            recover_time_ms INTEGER,
            ack_time_ms INTEGER,

            status TEXT NOT NULL,
            state TEXT,
            acked INTEGER DEFAULT 0,

            trigger_value REAL,
            value REAL,
            threshold_value REAL,

            error_message TEXT
        );
        )SQL"
    };
}

std::vector<std::string> DatabaseSchema::indexSqlList()
{
    return {
        // 历史曲线查询高频使用：按 point_id 查某个点位，并按 timestamp_ms 做时间范围过滤和排序。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_history_point_time
        ON telemetry_history(point_id, timestamp_ms);
        )SQL",

        // 命令 ACK 通过 seq 回查命令记录，用于快速更新命令执行结果。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_command_log_seq
        ON command_log(seq);
        )SQL",

        // 设备定位核心索引：大部分设备查询、删除、恢复都按 gateway_id + port_id + device_id 定位。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_device_location
        ON device(gateway_id, port_id, device_id);
        )SQL",

        // 最新点位按具体设备查询或删除，匹配 gateway_id + port_id + device_id 条件。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_latest_device
        ON latest_point(gateway_id, port_id, device_id);
        )SQL",

        // 最新点位按网关端口查询或批量删除，匹配 gateway_id + port_id 条件。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_latest_gateway
        ON latest_point(gateway_id, port_id);
        )SQL",

        // 设备离线检测会按 status 筛选非 offline 设备，减少全表扫描。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_device_status_state
        ON device_status(status);
        )SQL",

        // 网关心跳超时检测会按 online 状态筛选网关。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_gateway_status_state
        ON gateway_status(status);
        )SQL",

        // 告警更新和确认通常按 point_id 找当前告警，再结合 status 判断是否仍处于活跃状态。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_alarm_point_status
        ON alarm_event(point_id, status);
        )SQL",

        // 告警清理会按 status 删除已恢复或已确认告警。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_alarm_status
        ON alarm_event(status);
        )SQL",

        // 端口列表或状态筛选可按 gateway_id + status 查询某个网关下的端口。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_gateway_port_gateway
        ON gateway_port(gateway_id, status);
        )SQL",

        // 点位配置按工厂、区域、网关、端口、设备层级组织，用于层级筛选点位。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_point_config_device
        ON point_config(factory_id, area_id, gateway_id, port_id, device_id);
        )SQL",

        // 预留给按网关筛选告警状态的场景，例如查看某个网关下的 active 告警。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_alarm_gateway_status
        ON alarm_event(gateway_id, status);
        )SQL",

        // 预留给按网关查看历史数据的场景，支持 gateway_id + 时间范围查询。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_history_gateway_time
        ON telemetry_history(gateway_id, timestamp_ms);
        )SQL",

        // 预留给工厂级报表或统计，支持 factory_id + 时间范围查询。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_history_factory_time
        ON telemetry_history(factory_id, timestamp_ms);
        )SQL",

        // 预留给按注册状态筛选网关的场景，例如只看 online/offline 网关注册记录。
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_gateway_registry_status
        ON gateway_registry(status);
        )SQL"
    };
}
