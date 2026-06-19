---
name: imx6ull-code-editing
description: Edit the user's IMX6ULL/Qt/PC repo. Use for Linux_data, Linux_ui, Pc_data, Pc_ui, Pc_mqtt, IPC, JSON, MQTT, Modbus, RS485, Qt UI, database, device registry, ACK, snapshot, or project-structure code changes. Static inspection and code edits only; never build, run, test, deploy, SSH, start services, run MQTT/serial/RS485/hardware commands, or execute hardware-facing commands unless explicitly requested.
---

# IMX6ULL Code Editing

## Must Follow

- Inspect relevant files before editing.
- Make the smallest safe change only.
- Preserve user changes. Do not revert, overwrite, format, or clean unrelated files.
- Do not reorganize folders, rename files/classes, or change build structure unless explicitly requested.
- Use read-only inspection freely: `rg`, file reads, directory listings, `git status`.
- Do not build, run, test, deploy, SSH, start services, run Qt Creator steps, serial/RS485/MQTT commands, database mutation commands, or hardware-facing commands unless explicitly requested.
- If no build/test/runtime validation was run, say so clearly.
- The fixed protocol rules in `references/project-guidance.md` are higher priority than any local refactor idea.
- Do not change MQTT topic design unless the user explicitly requests a topic redesign.
- Do not change business semantics for ACK, telemetry, registration, snapshot, or device deletion unless explicitly requested.
- For MQTT/IPC/JSON changes, inspect both producer and consumer sides before editing.
- For Pc_data/Pc_ui/Linux_data/Linux_ui cross-module changes, read `references/project-guidance.md` first.
- Do not treat MQTT publish success as business success.
- Do not allow telemetry to create devices or point_config.
- Do not let UI directly trigger MQTT snapshot unless explicitly requested.
- Preserve legacy field compatibility when possible, but new protocol rules take priority.

## Environment

Real paths for context only:

- `Linux_data`: `debian@192.168.10.50:/home/debian/Linux_data`
- `Linux_ui`: `zhengpeng@192.168.17.139:/work/Qt_Object/qt_hmi`

The current AI/Codex environment cannot reliably build or run this project.

## Modules

- `Linux_data`: back-end data collection, IPC server, MQTT, Modbus, RS485, sensors, port management, service threads.
- `Linux_ui`: embedded Qt HMI, pages, QSS, IPC client.
- `Pc_data`: Windows PC-side data service, MQTT client, SQLite database, IPC server, command forwarding, ACK correlation, telemetry filtering, registry/snapshot management.
- `Pc_ui`: Windows PC-side Qt UI, communicates only with Pc_data through IPC, does not directly use MQTT or database.
- `Pc_mqtt`: old or auxiliary PC-side Qt/MQTT monitor if present.

Read `references/project-guidance.md` before any task involving MQTT topics, IPC/JSON contracts, ACK semantics, registration, telemetry, snapshot, device lifecycle, tombstone, database registry tables, Pc_data/Pc_ui/Linux_data/Linux_ui cross-module changes, or project architecture.

## Fixed Protocol Rules

These rules must not be changed unless the user explicitly asks for a protocol redesign.

MQTT topics are fixed:

- Registration entry: `gateway/register`
- Gateway upstream: `gateway/{gatewayId}/up`
- Port/device upstream: `gateway/{gatewayId}/{portId}/up`
- Gateway command: `cmd/{gatewayId}`
- Port/device command: `cmd/{gatewayId}/{portId}`

Registration messages must go to `gateway/register`, including:

- `gateway_register`
- `port_register`
- `device_register`
- `device_config_snapshot`
- `config_snapshot`

Telemetry must go to `gateway/{gatewayId}/{portId}/up`.

Commands must go to `cmd/{gatewayId}` or `cmd/{gatewayId}/{portId}`.

Telemetry must never create `device` or `point_config`.

Pc_ui must not directly connect to MQTT, access the database, or trigger MQTT snapshot.

MQTT publish success is transport success only. Business success requires Linux_data final ACK:

`stage=done && ok=true && status=success`

Device deletion must use persistent tombstone rules. Deleted devices must not be revived by telemetry, device_register, or normal snapshot. Only a successful add_device final ACK may reactivate a deleted device.

## IPC / JSON Rule

`Linux_data` is the source of truth for IPC/JSON.

Before changing communication between `Linux_data` and `Linux_ui`, inspect the back-end first, especially:

- `Linux_data/data/protocol.h`
- `Linux_data/data/command.c`

`Linux_ui` must follow the command names, response names, JSON fields, status values, and reason strings produced or expected by `Linux_data`.

Do not guess UI-side fields. Do not invent front-end-only JSON fields unless the back-end supports them. If the UI needs missing data, report the mismatch and suggest the smallest back-end change.

For Pc_data/Pc_ui/Linux_data MQTT and IPC flow, inspect both sender and receiver sides before editing. Do not change one side of the JSON contract without checking the other side.

## Qt / Thread Rules

- For Qt UI changes, check signal-slot connections, page creation, navigation, IPC callbacks, loading/empty/error states, and data update paths.
- Do not block the UI thread.
- Do not add busy loops or blocking sleeps.
- Keep serial, RS485, Modbus, MQTT, IPC, polling, and long-running work out of QWidget code.
- When changing polling/thread logic, check mutexes, lifetime, stop flags, timeouts, reconnect behavior, and whether the change affects one port, one master, one slave, or all ports.

## Final Response

Always end with:

- What changed.
- Which files changed.
- What was not run or validated.
- Risks or hidden issues.
- Whether IPC/JSON contract was checked.
- Whether fixed protocol rules were preserved.
- Suggested checks for the user to run next.

For review-only tasks, do not edit files. Say no code was modified and give a clear modification checklist.
