# PowerA PC HQ for macOS - System Architecture

This document describes the software architecture, modular components, and inter-process communication for **PowerA PC HQ for macOS**.

---

## Architecture Diagram

```mermaid
graph TD
    subgraph Hardware ["Hardware Layer (PowerA OPS v1 / 2.4G Dongle)"]
        INTF0["USB Interface 0 (Class 0xFF - XInput Stream)"]
        INTF1["USB Interface 1 (Class 0x03 - HID Config Col04)"]
        BT_DEV["Bluetooth HID Radio"]
    end

    subgraph Driver ["macOS Driver Subsystem (powera-driver)"]
        IOKIT_INTF0["Direct IOKit Engine (IOUSBInterfaceInterface)<br/>• Pipe 1: 20-byte XInput Reports<br/>• Pipe 2: White Status LED Packet"]
        HIDAPI_INTF1["hidapi Configuration Engine<br/>• 0xE1 Commands (RGB, Rumble, Profiles)"]
        BT_INPUT["Bluetooth HID Parser"]
        INPUT_LOOP["High-Priority Polling Thread (1000Hz)"]
        IPC_SRV["Local IPC Server (Port 8765 /api/status)"]
        VIRT_BRIDGE["Karabiner VirtualHIDDevice Bridge"]
        SDL_MAP["SDL GameController DB Auto-Sync"]
    end

    subgraph Games ["macOS Gaming Ecosystem"]
        STEAM["Steam (macOS)"]
        NATIVE_GAMES["Native Mac Games (Apple Arcade)"]
        EMU["Emulators (RetroArch, Ryujinx, RPCS3)"]
    end

    subgraph GUI ["macOS Native App (PowerA PC HQ.app)"]
        COCOA["Cocoa NSWindow & Menu Bar StatusItem"]
        WEBVIEW["WKWebView Accelerated Renderer"]
        FRONTEND["HTML5 / Modern Dark CSS / JS Engine"]
    end

    INTF0 -->|Interrupt IN Pipe 1| IOKIT_INTF0
    IOKIT_INTF0 -->|LED Command (0x01 0x03 0x06)| INTF0
    INTF1 <-->|Vendor Reports (0xE1)| HIDAPI_INTF1
    BT_DEV -->|GATT HID| BT_INPUT

    IOKIT_INTF0 --> INPUT_LOOP
    BT_INPUT --> INPUT_LOOP
    HIDAPI_INTF1 --> INPUT_LOOP

    INPUT_LOOP --> IPC_SRV
    INPUT_LOOP --> VIRT_BRIDGE
    VIRT_BRIDGE --> NATIVE_GAMES
    SDL_MAP --> STEAM
    SDL_MAP --> EMU

    IPC_SRV <-->|HTTP / JSON Telemetry (40Hz)| FRONTEND
    COCOA --> WEBVIEW
    WEBVIEW --> FRONTEND
```

---

## 1. Modular Subsystems

### A. Driver Daemon (`driver/powera_driver.c`)
- **Language**: C99 with POSIX threads.
- **Dual-Interface Hardware Architecture**:
  - **Interface 0 (Input Streaming & LED)**: Claims USB Interface 0 (`bInterfaceClass: 0xFF`, Xbox 360 controller) directly via native user-space Apple IOKit (`IOUSBInterfaceInterface` via `kIOUSBInterfaceUserClientTypeID`). Reads raw 20-byte XInput packets (`0x00 0x14 ...`) from Interrupt IN Pipe 1, bypassing macOS `IOHIDManager` which ignores Class 0xFF. Distinguishes IOKit interrupt timeouts (`0xE00002ED` / `0xE00002C2`) from true hardware disconnects (`0xE00002C0` / `0xE00002CD`). Automatically transmits the XID `GET_CAPABILITIES` control transfer (`0xC1, 0x01, 0x0100, 0x0000, 20`) on Endpoint 0 to wake the controller from standby, and issues the solid white status LED command (`0x01 0x03 0x02`) on Pipe 2 upon connection.
  - **Interface 1 (Configuration Channel)**: Claims USB Interface 1 (`bInterfaceClass: 0x03` HID, `Col04`) via `hidapi` using exact USB device path enumeration (`hid_open_path`) to handle bidirectional `0xE1` configuration frames (RGB lighting, profile import/export, trigger calibration).
  - **Bluetooth Fallback**: In Bluetooth mode, decodes standard GATT HID reports via `hidapi`.
- **Concurrency & IPC**:
  - Implements POSIX `flock` file locking (`/tmp/powera_driver.lock`) ensuring a strict single-instance lifecycle and preventing competing process lockouts (`kIOReturnExclusiveAccess` `0xE00002C5`).
  - Streams low-latency decoded telemetry (normalized thumbsticks, analog triggers, digital buttons, and rear paddles) over `http://127.0.0.1:8765/api/status` at 40Hz.
  - Handles hotplug events: upon USB disconnect, cleanly releases IOKit and IOHID handles and returns to non-blocking discovery polling; upon reconnection, re-establishes the connection in < 300ms.

### B. Hardware Protocol & Serialization (`driver/powera_protocol.c`)
- **Role**:
  - Implements the exact 16-bit CRC algorithm decompiled from `PowerA_PC_HQ.exe`.
  - Encapsulates 64-byte `0xE1` command packets.
  - Formats rumble commands (Interface 0 Pipe 2 XInput dual-motor rumble and vendor frames).
  - Serializes and deserializes authentic 173-byte `.pwa` profile structures.

### C. Native Application Wrapper (`src/main.m`)
- **Language**: Objective-C with Apple Cocoa & WebKit frameworks.
- **Role**:
  - Launches and supervises the background driver daemon.
  - Manages the native borderless `NSWindow` (`NSWindowStyleMaskFullSizeContentView`, `setTitlebarAppearsTransparent:YES`) with dark aqua appearance.
  - Native window traffic light controls (close, minimize, zoom) sit seamlessly over the titlebar.
  - Hosts `WKWebView` with zero-overhead native script message handlers and transparent background rendering to eliminate white flashes.
  - Installs a native macOS Menu Bar Status Item for quick profile switching and battery readout.

### D. User Interface Layer (`src/ui/`)
- **Technologies**: HTML5, Modern CSS (macOS Glassmorphism, CSS Custom Properties, Flexbox/Grid), JavaScript.
- **Role**:
  - Full macOS native titlebar integration: utilizes `-webkit-app-region: drag` for fluid window movement and an 88px left margin accommodating the native traffic lights without overlapping controls or duplicate dots.
  - High-resolution authentic asset binding: links directly to `../assets/` within the application bundle for authentic PowerA OPS v1 (Model 1531) renders:
    - `controller_front.png` (`Controller_1531_2.png`, 96,850 B): Matte black front chassis with hall-effect sticks, blue ABXY, and PowerA logo.
    - `controller_back.png` (`Controller_Back.png`, 84,323 B): Rear casing with textured grip and 4 back paddles (AGL1, AGL2, AGR1, AGR2).
    - `controller_back_trigger.png` (`Controller_Back_Trigger.png`, 81,065 B): Physical 3-way trigger lock switches.
  - Interactive Front/Back view switcher with smooth opacity cross-fading.
  - Live 60fps dual analog stick radar canvas with real-time Cartesian coordinates.
  - 3-way trigger lock meters (T1, T2, T3) and deadzone sliders.
  - Dual-rumble independent vibration strength sliders (Left Heavy & Right Light) and instant hardware testers.
  - RGB studio with animated glow preview and custom color palette.
  - Active Hardware Input Tester tab with real-time digital button matrix, AGL1-AGR2 back paddle badges, dual precision stick radar scopes, polar magnitude/angle readouts, trigger travel meters, and live 15-byte hex telemetry stream.
  - `.pwa` profile management (import/export).
