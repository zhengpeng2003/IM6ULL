- - # IMX6ULL Project Guidance

    This file contains detailed guidance for the user's IMX6ULL/Qt repository. Read it only when the task crosses modules, changes communication flow, changes architecture, asks about industrial realism, compares projects, or needs deeper project context.

    ## Real Environment

    The real project is split across machines:

    - `Linux_data`: `debian@192.168.10.50:/home/debian/Linux_data`
    - `Linux_ui`: `zhengpeng@192.168.17.139:/work/Qt_Object/qt_hmi`

    These paths are for context only.

    Do not SSH, build, run, test, deploy, start services, access serial ports, access RS485, run MQTT services, or execute hardware-facing commands unless the user explicitly requests that action in the current turn.

    The current AI/Codex environment cannot reliably build or run this project. Default to static inspection and code edits only.

    ## Repository Modules

    - `Linux_data`: i.MX6ULL-side data collection, device integration, IPC server, MQTT, Modbus, RS485, sensors, ports, and service threads.
    - `Linux_ui`: embedded Qt HMI, status/trend/settings/info pages, QSS style, and IPC client.
    - `Pc_mqtt`: PC-side Qt/MQTT monitoring tool.

    ## Industrial Reality and Optimization Guidance

    When the user asks whether a design is realistic for industrial environments, compare the current implementation with common industrial practice and provide practical optimization suggestions.

    General principles:

    - Industrial systems usually prioritize stability, maintainability, observability, and predictable failure handling over fancy UI effects.
    - Do not assume every RS485 device can be auto-detected. Many RS485/Modbus RTU devices require manual configuration of port, baud rate, parity, stop bits, slave address, function code, and register map.
    - Treat RS485 as a bus with one master polling multiple slave devices. Avoid assuming device discovery works like USB plug-and-play.
    - For multi-port RS485 systems, prefer clear separation of each port/master state, polling thread, timeout, retry count, and device list.
    - Avoid letting one slow or offline slave block the entire system for too long.
    - Use timeout, retry, offline marking, and recovery logic explicitly.
    - Use configuration files or persistent settings for device lists, register maps, polling intervals, and alarm thresholds.
    - For many devices, avoid polling every register at the same high frequency. Group registers, adjust polling intervals by importance, and separate fast/slow data.
    - Keep real-time acquisition, data processing, IPC/MQTT publishing, database storage, and UI rendering separated.
    - The embedded device should handle field acquisition, control, device state, and local safety logic. The PC side should usually focus on monitoring, analysis, history, configuration, and optional debug/control modes.
    - MQTT is suitable for telemetry and event reporting, but should include reconnect, offline buffering if needed, topic design, QoS choice, retained message usage, and payload versioning.
    - For control commands, include ACK/result responses and avoid fire-and-forget behavior.
    - For alarms, use a lifecycle model: active, acknowledged, recovered, cleared, and logged.
    - For UI, show clear states: normal, warning, alarm, offline, communication timeout, empty data, loading, and configuration error.
  
    When suggesting optimization plans, prefer practical staged improvements:

    1. Fix correctness and data contract issues first.
    2. Stabilize communication and error handling.
    3. Add configuration persistence.
    4. Add device online/offline state management.
    5. Add alarm lifecycle and logging.
    6. Add history storage and trend analysis.
    7. Improve UI display and usability.
    8. Add MQTT reliability and PC-side monitoring.
    9. Add documentation, README, architecture diagram, and protocol documentation.
  
    When comparing the user's project with another project:
  
    - Compare architecture, communication flow, real device integration, configuration ability, reliability, UI completeness, data storage, alarm handling, and deployment realism.
    - Be direct about what is missing, but provide a feasible improvement path.
    - Avoid suggesting unrealistic enterprise-level features if they do not match the user's time, hardware, or interview goals.
    - Prioritize improvements that are useful for internship interviews and can be explained clearly.
  
    ## IPC and JSON Contract
  
    `Linux_data` is the source of truth for IPC and JSON communication.
  
    When changing communication between `Linux_data` and `Linux_ui`, the front-end must adapt to the back-end format unless the user explicitly asks to change the back-end protocol.
  
    Pay special attention to:
  
    - `Linux_data/data/protocol.h`
    - `Linux_data/data/command.c`

    Rules:

    - Inspect the back-end implementation before changing IPC or JSON logic.
    - When `Linux_ui` sends data or commands to `Linux_data`, use the format expected by the back-end.
    - When `Linux_ui` parses responses from `Linux_data`, use the format actually produced by the back-end.
    - Do not guess IPC or JSON fields from UI expectations.
    - Do not invent front-end-only JSON fields unless the back-end already supports them.
    - Do not rename JSON fields in `Linux_ui` without checking and updating the matching back-end code.
    - If the UI needs a field that the back-end does not provide, report the mismatch and suggest the smallest back-end change.
    - If a JSON field, command type, status value, reason string, or data structure is added, removed, or renamed, inspect both sender and receiver sides.
    - Do not only change the UI or only change the back-end if the data contract crosses both modules.
  
    Common areas that must stay consistent:
  
    - command names
    - response names
    - JSON field names
    - device id fields
    - port fields
    - baud rate fields
    - slot/master index fields
    - connected/disconnected status fields
    - error/reason fields
    - scan result fields
    - device telemetry fields
  
    ## Qt UI Rules
  
    When editing `Linux_ui` or `Pc_mqtt` Qt code:
  
    - Check signal-slot connections when changing buttons, pages, navigation, timers, or IPC callbacks.
    - Do not only change the visual widget. Also check whether the data update path is connected.
    - Keep UI states explicit: loading, empty, connected, disconnected, error, timeout.
    - Avoid blocking the UI thread.
    - Do not add long polling, sleep, serial-port access, RS485 access, MQTT service loops, or hardware access directly inside QWidget code.
    - When changing a page, check whether the page is created, added to the stack/layout, and connected to navigation.
    - When changing loading animations, status labels, buttons, or cards, check both the initial state and the update state.
    - Prefer clear UI state functions such as `showLoadingState`, `showEmptyState`, `showErrorState`, or existing equivalent functions if the project already has them.
    - Keep the 480x272 embedded screen constraint in mind for `Linux_ui`.

    ## Threading and Polling Rules
  
    When editing polling, serial port, MQTT, IPC, Modbus, RS485, or service-thread logic:
  
    - Do not add busy loops.
    - Do not add blocking sleeps in UI code.
    - Keep long-running hardware, serial-port, polling, MQTT, or IPC work out of the Qt UI thread.
    - Check mutexes, object lifetime, stop flags, and thread ownership before changing thread logic.
    - If changing polling behavior, explain whether it affects one port, one slot, one master, one slave device, or all ports.
    - Do not silently change the polling interval, retry count, timeout behavior, or reconnect behavior unless the user asks or the bug requires it.
    - When changing start/stop logic, check whether old threads, timers, file descriptors, sockets, or serial ports are safely stopped and released.
    - When changing multiple RS485 ports or masters, keep each port/master state isolated unless the existing design intentionally shares state.
  
    ## Linux_data Rules
  
    When editing `Linux_data`:

    - Keep service-side logic focused on data collection, device integration, IPC server, MQTT, Modbus, sensors, ports, and service threads.
    - Treat `Linux_data` as the authority for real device data and IPC/JSON protocol definitions.
    - Do not add UI-specific assumptions into `Linux_data` unless the protocol explicitly requires them.
    - When changing command handling, inspect the command parser, response builder, and related protocol definitions.
    - When changing device telemetry, inspect both packing and unpacking paths.
    - When changing port management, inspect scan, connect, disconnect, status reporting, and polling behavior.
    - Be careful with C/C++ boundaries. Do not call C++ classes directly from C files unless the existing project already exposes an `extern "C"` wrapper.
  
    ## Linux_ui Rules
  
    When editing `Linux_ui`:
  
    - Treat `Linux_ui` as the embedded Qt HMI front-end.
    - Do not invent JSON fields to satisfy UI display needs. First inspect what `Linux_data` actually provides.
    - Check IPC client send and receive paths when changing pages that depend on back-end data.
    - Check signal-slot connections for page buttons, navigation buttons, scan buttons, connect buttons, disconnect buttons, timers, and IPC callbacks.
    - Avoid heavy work in QWidget constructors, paint events, or button callbacks.
    - If a page shows data from `Linux_data`, check both the empty/default state and the data-updated state.
  
    ## Pc_mqtt Rules
  
    When editing `Pc_mqtt`:
  
    - Treat `Pc_mqtt` as the PC-side Qt/MQTT monitoring tool.
    - Keep PC-side monitoring separate from embedded `Linux_ui` behavior unless the user asks to share logic.
    - Do not assume PC-side fields are available on the embedded IPC protocol unless they are actually provided by `Linux_data`.
    - When changing MQTT topics, payloads, or parsing logic, check producer and consumer expectations.
    - Do not silently change topic names, QoS, reconnect behavior, database fields, or chart update behavior without explaining the impact.
  
    ## Editing Workflow
  
    1. Inspect the relevant files.
    2. Identify the smallest safe change.
    3. Make only the required code edits.
    4. Avoid broad refactors unless explicitly requested.
    5. If the change crosses modules, keep data contracts explicit and update both sides consistently.
    6. If the change touches communication flow, check `Linux_data` protocol and command handling first.
    7. Before finishing, review changed files enough to catch obvious syntax, include, naming, signal-slot, JSON field, or contract mistakes without running the project.
    8. If no build, test, or runtime validation was performed, state that clearly.
  
    ## Review-Only Workflow
  
    If the user asks for analysis, inspection, or a modification checklist without asking for direct code edits:
  
    - Do not modify files.
    - Inspect relevant files only.
    - Explain the current flow clearly.
    - Identify likely bugs, missing connections, mismatched fields, or risky logic.
    - Provide a concrete modification checklist the user can follow later.
    - If the issue crosses `Linux_data` and `Linux_ui`, describe both sides of the data flow.
    - If the issue depends on runtime or hardware behavior, state that static inspection cannot fully verify it.
