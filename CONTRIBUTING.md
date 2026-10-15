# Contributing to PowerA PC HQ for macOS

Thank you for your interest in contributing to **PowerA PC HQ for macOS**! This is an independent, unofficial personal side project with no ties to PowerA or ACCO Brands. Community contributions, bug fixes, and documentation improvements are warmly welcomed.

---

## Code of Conduct

By participating in this project, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md). Please treat all contributors with respect and courtesy.

---

## Development Prerequisites

To build and contribute to the project, ensure you have:

1. **macOS**: macOS 11.0 (Big Sur) or newer (tested on macOS 14 Sonoma and macOS 15 Sequoia).
2. **Apple Silicon or Intel Mac**: Universal architecture support.
3. **Xcode Command Line Tools**:
   ```bash
   xcode-select --install
   ```
4. **Homebrew & `hidapi`**:
   ```bash
   brew install hidapi
   ```

---

## Building from Source

The repository includes a top-level `Makefile` for streamlined development:

```bash
# 1. Clone the repository
git clone https://github.com/YOUR_USERNAME/PowerA-PC-HQ-macOS.git
cd PowerA-PC-HQ-macOS

# 2. Verify build prerequisites
make test

# 3. Build the native driver and .app bundle
make app

# 4. (Optional) Build standalone drag-and-drop DMG installer
make dmg

# 5. (Optional) Build CLI hardware diagnosis tools
make tools
```

Output binaries will be placed in `bin/`, and the compiled application will be in `build/PowerA PC HQ.app`.

---

## Running & Testing

### Running the GUI Application
```bash
open "build/PowerA PC HQ.app"
```

### Running the Driver Daemon in Foreground
```bash
./bin/powera-driver
```

### Checking Driver Telemetry
When running, the driver exposes a local JSON telemetry server on `http://127.0.0.1:8765`:
```bash
curl -s http://127.0.0.1:8765/api/status | jq .
```

### Testing Controller Inputs
To run the interactive CLI input listener:
```bash
make tools
./bin/powera_active_listener
```

---

## Subsystems & Architecture

- **`driver/`**: High-performance C99 daemon utilizing direct user-space Apple IOKit (`IOUSBInterfaceInterface`) for Interface 0 (Interrupt IN input stream & white LED control) and `hidapi` for Interface 1 (RGB lighting, vibration, profile import/export).
- **`src/`**: Native Objective-C Cocoa wrapper hosting a frameless, transparent-titlebar `WKWebView`.
- **`src/ui/`**: Responsive glassmorphism interface built with modern HTML5, CSS custom properties, and vanilla JavaScript (no heavy node/npm dependencies).
- **`tools/`**: Diagnostic utilities and the Karabiner-Elements input bridge (`powera_karabiner_bridge.cpp` / `.py`).
- **`docs/`**: Detailed technical specifications for architecture, driver behavior, protocols, and input mapping.

---

## Pull Request Guidelines

1. **Fork & Branch**: Create a descriptive feature branch from `main` (e.g., `git checkout -b feature/bluetooth-battery-profile`).
2. **Keep Commits Focused**: Write clear, imperative commit messages (e.g., `Fix trigger deadzone serialization in .pwa profiles`).
3. **Update Documentation**: When modifying protocols, driver endpoints, or features, ensure corresponding documents in `docs/` and `README.md` are updated.
4. **Test on Real Hardware**: If you have a PowerA controller (OPS v1, OPS v3 Pro, or dongle), test both wired and wireless modes.
5. **Open a Pull Request**: Submit your PR with a clear summary of changes, motivation, and test steps performed.
