---
name: imx6ull-code-editing
description: Edit the user's IMX6ULL/Qt repo. Use for Linux_data, Linux_ui, Pc_mqtt, IPC, JSON, MQTT, Modbus, RS485, Qt UI, or project-structure code changes. Static inspection and code edits only; never build, run, test, deploy, SSH, start services, or execute hardware-facing commands unless explicitly requested.
---

# IMX6ULL Code Editing

## Must Follow

- Inspect relevant files before editing.
- Make the smallest safe change only.
- Preserve user changes. Do not revert, overwrite, format, or clean unrelated files.
- Do not reorganize folders, rename files/classes, or change build structure unless explicitly requested.
- Use read-only inspection freely: `rg`, file reads, directory listings, `git status`.
- Do not build, run, test, deploy, SSH, start services, run Qt Creator steps, serial/RS485/MQTT commands, or hardware-facing commands unless explicitly requested.
- If no build/test/runtime validation was run, say so clearly.

## Environment

Real paths for context only:

- `Linux_data`: `debian@192.168.10.50:/home/debian/Linux_data`
- `Linux_ui`: `zhengpeng@192.168.17.139:/work/Qt_Object/qt_hmi`

The current AI/Codex environment cannot reliably build or run this project.

## Modules

- `Linux_data`: back-end data collection, IPC server, MQTT, Modbus, RS485, sensors, port management, service threads.
- `Linux_ui`: embedded Qt HMI, pages, QSS, IPC client.
- `Pc_mqtt`: PC-side Qt/MQTT monitor.

Read `references/project-guidance.md` only when the task crosses modules, changes communication flow, changes architecture, or needs detailed project rules.

## IPC / JSON Rule

`Linux_data` is the source of truth for IPC/JSON.

Before changing communication between `Linux_data` and `Linux_ui`, inspect the back-end first, especially:

- `Linux_data/data/protocol.h`
- `Linux_data/data/command.c`

`Linux_ui` must follow the command names, response names, JSON fields, status values, and reason strings produced or expected by `Linux_data`.

Do not guess UI-side fields. Do not invent front-end-only JSON fields unless the back-end supports them. If the UI needs missing data, report the mismatch and suggest the smallest back-end change.

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
- Suggested checks for the user to run next.

For review-only tasks, do not edit files. Say no code was modified and give a clear modification checklist.
