#include "DatabaseSchema.hpp"

std::vector<std::string> DatabaseSchema::tableSqlList()
{
    return {
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
            recover_time_ms INTEGER,

            status TEXT NOT NULL,

            trigger_value REAL,
            threshold_value REAL,

            error_message TEXT
        );
        )SQL"
    };
}

std::vector<std::string> DatabaseSchema::indexSqlList()
{
    return {
        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_gateway_status_state
        ON gateway_status(status);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_gateway_port_gateway
        ON gateway_port(gateway_id, status);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_command_log_seq
        ON command_log(seq);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_device_location
        ON device(gateway_id, port_id, device_id);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_device_status_state
        ON device_status(status);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_point_config_device
        ON point_config(factory_id, area_id, gateway_id, port_id, device_id);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_latest_gateway
        ON latest_point(gateway_id, port_id);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_latest_device
        ON latest_point(gateway_id, port_id, device_id);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_history_point_time
        ON telemetry_history(point_id, timestamp_ms);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_history_gateway_time
        ON telemetry_history(gateway_id, timestamp_ms);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_history_factory_time
        ON telemetry_history(factory_id, timestamp_ms);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_alarm_status
        ON alarm_event(status);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_alarm_point_status
        ON alarm_event(point_id, status);
        )SQL",

        R"SQL(
        CREATE INDEX IF NOT EXISTS idx_alarm_gateway_status
        ON alarm_event(gateway_id, status);
        )SQL"
    };
}
