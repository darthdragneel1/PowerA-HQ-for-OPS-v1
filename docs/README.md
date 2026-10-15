# PowerA PC HQ for macOS

[![Platform: macOS](https://img.shields.io/badge/Platform-macOS%2011%2B-brightgreen.svg)](https://apple.com/macos)
[![Architecture: Apple Silicon & Intel](https://img.shields.io/badge/Architecture-Universal%20(arm64%20%7C%20x86__64)-orange.svg)]()
[![Build & Test](https://github.com/abhishiekkurra/PowerA-PC-HQ-macOS/actions/workflows/build.yml/badge.svg)](https://github.com/abhishiekkurra/PowerA-PC-HQ-macOS/actions/workflows/build.yml)

> **Note**: This is an **unofficial, independent personal side project** created out of personal interest to bring native driver support and companion tooling to macOS. The author has **no affiliation, connection, or ties to PowerA or ACCO Brands**.

A high-performance, native macOS companion application and low-latency driver subsystem for the **PowerA OPS v1 Wireless Controller** (and compatible models), built by reverse-engineering the official Windows hardware protocol and `.pwa` profile architecture.

Designed natively for Apple Silicon (M1/M2/M3/M4) and Intel Macs running macOS 11.0 (Big Sur) through macOS 15+ (Sequoia).

---

## Highlights & Capabilities

- **100% Native macOS Application**: Built natively in Objective-C / Cocoa and C, compiled directly into a self-contained macOS Application Bundle (`PowerA PC HQ.app`).
- **Authentic macOS Window Integration**: Native transparent titlebar, full-window content view, smooth `-webkit-app-region: drag` handling, integrated Apple traffic-light controls (zero overlap or duplicate buttons), and premium dark glassmorphism aesthetic.
- **All Three Connection Modes Supported**:
  1. **Wired USB-C Mode** (`VID: 0x20D6`, `PID: 0x4025`): Up to 1000Hz polling rate with full rumble and lighting. Implements the USB XID `GET_CAPABILITIES` initialization handshake over Endpoint 0 to wake the controller from hardware standby and turn on the center white LED.
  2. **2.4 GHz Wireless Dongle Mode** (`PID: 0x400C / 0x4013 / 0x401E / 0x4022 / 0x4027 / 0x404F`): Proprietary wireless adapter communication with wireless pairing sync.
  3. **Bluetooth Mode**: Standard Bluetooth HID integration with calibrated axis mapping.
- **Universal Game Compatibility**:
  - Automatically installs and synchronizes `SDL_GAMECONTROLLERCONFIG` and `gamecontrollerdb.txt` mappings into Steam, OpenEmu, RetroArch, Ryujinx, RPCS3, Dolphin, and native Mac games.
- **Advanced Gaming Buttons (AGL1, AGL2, AGR1, AGR2)**:
  - Full configuration and live testing of all 4 back grip paddles.
- **3-Way Trigger Stop Calibration**:
  - T1 (100% full travel), T2 (50% rapid fire travel), and T3 (hair-trigger instant digital click).
- **Sticks Calibration & Response Curves**:
  - Live 60fps radar displays, inner deadzones, outer deadzones, and 4 curve models (Linear, Smooth, Aggressive, Instant).
- **Active Hardware Input Tester**:
  - Live diagnostic panel testing every button (ABXY, D-pad, bumpers, system, stick clicks, and AGL1-AGR2 back paddles), dual thumbstick radar scopes, polar magnitude/angular heading readouts, analog trigger travel gauges, live polling rate (Hz), cumulative packet counter, and live raw telemetry stream.
- **Dual-Rumble Haptic Vibration Studio**:
  - Independent strength sliders and live testing for Left Grip (low-frequency heavy rotor) and Right Grip (high-frequency crisp rotor).
- **RGB Lighting Studio**:
  - Full lighting control across authentic OPS v1 zones (All Zones, PowerA Logo & Status LED, Thumbstick Halo, Grip Accents) with Solid, Breathing, Color Cycle, and Wave modes.
- **Authentic PowerA OPS v1 Assets & Graphics**:
  - Embedded high-resolution matte black controller chassis renders, hall-effect thumbsticks, rear grip paddle ergonomics, and 3-way trigger stops.
- **Karabiner-Elements Input Bridge**:
  - Includes both a native C++ (`powera-karabiner-bridge`) and Python (`powera_karabiner_bridge.py`) background tool that translates controller reports and back paddles into macOS keyboard strokes, mouse clicks, and Karabiner complex modifications.
- **Profile Management (.pwa)**:
  - Full binary compatibility with authentic 173-byte `.pwa` profiles (`T1 Profile.pwa`, `T2 Profile.pwa`, `T3 Profile.pwa`).

---

## Quick Start Guide

### 1. Installation via Drag-and-Drop DMG

Download the latest release DMG from the [Releases](https://github.com/abhishiekkurra/PowerA-PC-HQ-macOS/releases) page:

1. Open `PowerA_PC_HQ_v1.4.2_macOS.dmg`.
2. Drag **PowerA PC HQ** into your **Applications** folder.
3. Launch **PowerA PC HQ** from Launchpad or Spotlight.

### 2. Building from Source

If you prefer building from source, ensure you have Homebrew and `hidapi` installed:

```bash
# Install dependencies
brew install hidapi

# Build application bundle and driver daemon
make app

# (Optional) Build standalone DMG installer
make dmg

# (Optional) Install driver service and app to ~/Applications
make install
```

### 3. Launching the Application

```bash
# Launch installed app
open -a "PowerA PC HQ"

# Or launch local build directly
open "build/PowerA PC HQ.app"
```

### 4. Running & Restarting the Driver Daemon

The driver daemon runs automatically in the background to handle controller hotplug events and telemetry:

```bash
# Restart background daemon cleanly
make restart
# or: ./scripts/restart_driver.sh

# Or run interactively in foreground:
./bin/powera-driver
```

When active, the driver streams real-time status and telemetry on `http://127.0.0.1:8765/api/status`.

---

## Build System (`Makefile`)

The repository includes a comprehensive `Makefile` for streamlined development:

| Command | Action |
|---|---|
| `make` / `make app` | Compiles native driver, bridge, and builds `build/PowerA PC HQ.app` |
| `make dmg` | Packages and codesigns the standalone `dist/*.dmg` installer |
| `make tools` | Compiles CLI diagnostic tools (`bin/powera_active_listener`, etc.) |
| `make install` | Deploys app to `~/Applications`, registers launchd agent, and configures SDL mappings |
| `make restart` | Gracefully restarts the background driver daemon |
| `make test` | Checks build prerequisites and Homebrew header availability |
| `make clean` | Removes `build/`, `dist/`, and `bin/` directories |
| `make help` | Displays list of all available Makefile targets |

---

## Repository Structure

```
PowerA-PC-HQ-macOS/
├── Makefile                      # Top-level unified build system
├── CONTRIBUTING.md               # Guidelines for contributors and PR submission
├── CODE_OF_CONDUCT.md            # Contributor Covenant Code of Conduct
├── SECURITY.md                   # Vulnerability disclosure policy
├── .gitignore                    # Git ignore rules for macOS & build artifacts
├── .github/                      # GitHub Actions CI & community issue templates
│   ├── workflows/                # Automated CI build and release pipelines
│   └── ISSUE_TEMPLATE/           # Bug report & feature request templates
├── driver/                       # Driver subsystem source code
│   ├── powera_protocol.h         # Reverse-engineered packet structures & opcodes
│   ├── powera_protocol.c         # Hardware CRC, 0xE1 packets, and .pwa serializer
│   ├── powera_driver.c           # IOKit & hidapi hardware communication engine
│   └── com.powera.driver.plist   # macOS launchd background service template
├── src/                          # macOS App wrapper and UI
│   ├── main.m                    # Cocoa / WebKit native wrapper with authentic dock icon
│   └── ui/                       # Modern dark glassmorphism interface
│       ├── index.html            # Main dashboard layout with connection guide banner
│       ├── style.css             # PowerA aesthetic styling & pulsing status dots
│       └── app.js                # Frontend engine, dynamic disconnect state & Gamepad API bridge
├── assets/                       # High-resolution artwork, logos & AppIcon.icns
├── profiles/                     # Authentic T1, T2, T3 .pwa profile binaries
├── tools/                        # CLI diagnostic utilities & Karabiner-Elements bridge
│   ├── powera_active_listener.c  # Multi-stage interactive input mapper
│   ├── powera_karabiner_bridge.cpp # High-speed C++ input injector
│   ├── powera_karabiner_bridge.py  # Python input injection alternative
│   ├── diag_live.c               # Low-level IOKit pipe inspector
│   └── interactive_tester.c      # Raw packet visualizer
├── scripts/                      # Build & automation scripts
│   ├── build_bundle.sh           # Builds .app bundle with embedded frameworks
│   ├── build_dmg.sh              # Builds the standalone drag-and-drop DMG installer
│   ├── restart_driver.sh         # Cleanly restarts the driver daemon
│   └── install_driver.sh         # Installs app & driver mappings
└── docs/                         # In-depth technical documentation
    ├── README.md                 # Documentation mirror
    ├── ARCHITECTURE.md           # System architecture & data flow
    ├── DRIVER_GUIDE.md           # macOS driver guide (Wired, 2.4G, Bluetooth)
    ├── KARABINER_GUIDE.md        # Karabiner-Elements & input injection guide
    └── PROTOCOL.md               # Reverse-engineered USB protocol & CRC
```

---

## Technical Documentation

Detailed deep-dive guides are available in the [`docs/`](docs/) directory:

- [**System Architecture (`docs/ARCHITECTURE.md`)**](docs/ARCHITECTURE.md): Multi-threaded IOKit / hidapi design, IPC architecture, and data flow.
- [**Driver & Hardware Guide (`docs/DRIVER_GUIDE.md`)**](docs/DRIVER_GUIDE.md): USB power states, XID wake-up handshake, 2.4G dongle pairing, and launchd background daemon configuration.
- [**Hardware Protocol Specification (`docs/PROTOCOL.md`)**](docs/PROTOCOL.md): 64-byte `0xE1` packet framing, 16-bit CRC algorithm, opcodes, and 173-byte `.pwa` profile layout.
- [**Karabiner-Elements Guide (`docs/KARABINER_GUIDE.md`)**](docs/KARABINER_GUIDE.md): How to translate back paddles and buttons into system keyboard shortcuts or mouse events.

---

## Contributing

We welcome contributions! Please read our [Contributing Guide](CONTRIBUTING.md) and [Code of Conduct](CODE_OF_CONDUCT.md) before submitting pull requests.

---

## Disclaimer & Third-Party Notice

This is an **unofficial, independent personal side project** created solely for personal interest, research, and controller interoperability on Apple macOS systems. 

**The author of this repository has no ties, connection, employment, endorsement, or affiliation with PowerA or ACCO Brands USA LLC.**

*All trademarks, product names, logos, and brands (including "PowerA", "OPS v1", and "OPS v3 Pro") are property of their respective owners. Their mention in this repository is strictly for descriptive compatibility and device identification purposes.*
