# PowerA OPS v1 macOS Driver Guide

This guide describes how the macOS driver subsystem communicates with the **PowerA OPS v1 Wireless Controller** across all three connection modes: **Wired USB-C**, **2.4 GHz Wireless Dongle**, and **Bluetooth**.

---

## 1. Connection Modes

### Mode 1: Wired USB-C (Direct Cable)

When connected via a USB-C to USB-A/C cable:
- **Vendor ID**: `0x20D6` (PowerA / Shenzhen Dashine Electronics)
- **Product ID**: `0x4025` (OPS v1)
- **USB Endpoints**:
  - **Interface 0 (EP 1 IN / OUT)**: Real-time controller reports (D-pad, sticks, analog triggers, standard buttons, and back paddles AGL1/AGL2/AGR1/AGR2). Polling rate up to 1000Hz (1ms).
  - **Interface 1 (Col04 / Vendor Feature Report)**: Configuration channel for sending 0xE1 packet frames, RGB lighting updates (`0xA6`), and profile read/write chunks (`0xAD`/`0xAE`). Rumble commands are dispatched via Interface 0 Pipe 2 to the dual grip motors.

### Mode 2: 2.4 GHz Wireless USB Dongle

When connected via the official PowerA 2.4GHz Wireless Adapter:
- **Product IDs**: `0x4027` (Official PowerA OPS v1 Wireless Controller - Adapter), `0x400C`, `0x4013`, `0x401E`, `0x4022`, or `0x404F`
- **Operation**:
  - The USB adapter handles 2.4GHz RF communication with the controller.
  - The driver talks directly to the adapter endpoints over USB, providing identical latency and full feature parity with wired mode.
  - Wireless pairing can be triggered via the app's "Pair 2.4G Dongle" button, which issues the `0xC2` opcode with the sync sequence.

### Hardware Connection Arbitration & Priority

When multiple PowerA USB endpoints are present simultaneously (for instance, if the 2.4GHz USB dongle is left plugged in while the controller is connected via USB-C cable):
- The driver implements **hierarchical device arbitration**:
  - **Priority 1**: Physical Wired Controller (`PID: 0x4025`) — Active USB data connection.
  - **Priority 2**: 2.4GHz Wireless Adapter (`PID: 0x4027`) — Wireless RF reception.
- Mode classification relies on exact Product IDs and interface descriptors rather than model name substrings, preventing false positives between `"PowerA OPS v1 Wireless Controller"` (the hardware model name used even over wired cable) and `"PowerA OPS v1 2.4GHz Wireless Adapter"`.

### Mode 3: Bluetooth Wireless Mode

When connected via standard Bluetooth:
- **Operation**:
  - macOS connects to the controller via Bluetooth HID over GATT (`UUID 0x1812`).
  - The driver monitors incoming reports and remaps the analog axes and back paddle buttons to ensure complete compatibility with macOS games that might otherwise misinterpret non-standard inputs.
  - Profile settings written to the controller's onboard flash memory in Wired/Dongle mode persist across Bluetooth sessions.

#### Bluetooth Pairing Protocol: Slow Flash vs. Rapid Strobe
The OPS v1 controller uses two distinct Bluetooth broadcast states:
1. **Slow Pulsing Blue LED (Reconnect Mode)**: The controller is attempting to reconnect to the *last bonded device* (e.g. Windows PC or previous host). In this state, macOS will discover the advertisement beacon, but the controller will abort L2CAP/SMP security key exchange and reject new bonding requests.
2. **Rapid Flashing Blue LED (Pairing Mode)**: Required for initial bonding. Achieved by holding the **top SYNC button** (adjacent to the USB-C port) for **3 to 5 seconds** until the blue light strobes rapidly.

#### Troubleshooting Bluetooth "Failed to Connect":
1. **Unplug USB & 2.4G Dongle**: Disconnect both the USB-C cable and the 2.4G dongle from your Mac so the controller does not prioritize USB bus data.
2. **Set Bottom Switch to `BT`**: Ensure the 3-position slider on the bottom edge is in the `BT` position.
3. **Clear Stale Host Pairing Table (Reset Pinhole)**: If previously paired to another device or if pairing failed, insert a paperclip into the small reset pinhole on the back plate for 5 seconds.
4. **Power On**: Press the Home (PowerA) button. The LED will illuminate blue.
5. **Enter Rapid-Strobe Pairing Mode**: Press and hold the top **SYNC button** for 3–5 seconds until the LED begins flashing **rapidly**.
6. **Forget Stale Cache in macOS**: In **System Settings > Bluetooth**, if "PowerA OPS v1" appears in the device list with a connection error, click the `(i)` button and select **Forget This Device**.
7. **Pair Freshly**: Select the controller when it appears in Nearby Devices; macOS will complete the secure link key exchange immediately.

---

## 2. Deep Dive: Why PowerA OPS v1 Shows "Connected" But Produces Zero Inputs on macOS

A frequent complaint among Mac users with PowerA controllers is that the device shows up as detected, yet **no stick movements, triggers, or button presses register anywhere in macOS or games**.

### Root Cause Analysis

#### A. USB Dual-Interface Architectural Split
The PowerA OPS v1 is an Xbox 360 / XInput controller over USB. It exposes **two physically separate USB interfaces**:
1. **Interface 0 (XInput Stream & Hardware LED)**:
   - `bInterfaceClass = 0xFF` (Vendor-Specific / Microsoft Xbox 360 Subclass `0x5D`, Protocol `0x01`).
   - Endpoint 1 IN (Interrupt): Continuously streams the 20-byte Xbox 360 controller report containing all thumbstick coordinates (16-bit signed), analog triggers (8-bit unsigned), digital buttons, and PowerA rear paddle bits (bytes 14–15).
   - Endpoint 2 OUT (Interrupt): Accepts the white status LED initialization command (`0x01 0x03 0x06`).
2. **Interface 1 (Vendor Configuration Channel / Col04)**:
   - `bInterfaceClass = 0x03` (Standard USB HID).
   - Usage Page: `0xFF00` (Vendor-defined feature reports).
   - Used by PowerA software for `0xE1` command packets (RGB lighting, profile import/export, and calibration).
   - **Crucial Fact**: Interface 1 **never sends stick or button input packets**.

#### B. macOS HID API Blindness to Class 0xFF
- Apple's `IOHIDFamily` (and by extension `hidapi` on macOS, which wraps `IOHIDManager`) **only enumerates standard USB HID devices (`bInterfaceClass == 0x03`)**.
- `IOHIDManager` completely ignores `bInterfaceClass == 0xFF`.
- Consequently, any tool relying solely on `hid_enumerate()` only detects **Interface 1**. The driver marks the controller as "Connected" (since Interface 1 opened successfully), but reading from Interface 1 times out indefinitely with 0 bytes.

#### C. macOS 15+ Native Driver Architecture (`XboxGamepad.dext`) & Vendor Lockdown
- In macOS 15+ (Sequoia / DriverKit 27), Apple introduced a native DriverKit extension: `/System/Library/DriverExtensions/XboxGamepad.dext`.
- However, Apple's `Info.plist` personality dictionary explicitly hardcodes **`idVendor: 1118` (`0x045E`, Microsoft)**:
  - `Xbox360Gamepad`: `idVendor: 0x045E`, `idProduct: 0x028E`
  - `XboxOneSGamepad`: `idVendor: 0x045E`, `idProduct: 0x02EA`
  - `XboxSeriesXGamepad`: `idVendor: 0x045E`, `idProduct: 0x0B12`
  - `XboxEliteV2Gamepad`: `idVendor: 0x045E`, `idProduct: 0x0B00`
- Even though third-party controllers (such as PowerA) speak the exact same Xbox GIP / XInput protocol, macOS **artificially refuses to bind `XboxGamepad.dext`** because the Vendor ID is `0x20D6` (or `0x24C6`) instead of `0x045E`.
- Consequently, Apple's `AppleUserHIDDevice` is never spawned, and `GCController` / WebKit `navigator.getGamepads()` remain completely oblivious to the controller.
- **Impact on Steam Input for macOS**: Steam on macOS historically used a kernel extension (`SteamInputX.kext`) to handle Xbox 360 controllers. On modern macOS (macOS 12+), Valve retired `SteamInputX.kext` and transitioned Steam Input to use Apple's native `GCController.framework`. Because Apple's framework rejects VID `0x20D6`, Steam's controller subsystem enumerates 0 connected controllers, even when "Enable Steam Input for Xbox controllers" or "Generic controllers" is enabled in Steam settings.

#### D. Comparison with `jki127/powera-xbox-mac` (Firmware Spoofing vs. User-Space Driver)
A community workaround developed by `jki127` demonstrated getting an older **PowerA Enhanced Wired Controller for Xbox One** (Model `1505660-03`, Weltrend WT60F168 chip) working on macOS 15+ by modifying its firmware:
- **How `powera-flash` worked**: It extracted a 64KB bare firmware image (`LBX902_V1.1.5_1.bin`) from a 2018 PowerA Windows updater, replaced the USB descriptor bytes (`24C6:581A`) with Microsoft's Xbox One S ID (`045E:02EA`), and flashed it via the controller's ISP bootloader (`2E2C:8008`). Tricked into seeing a Microsoft ID, macOS 15's `XboxGamepad.dext` loaded.
- **Why Firmware Flashing is NOT Feasible or Safe for PowerA OPS v1**:
  1. **Bricking Hazard**: The OPS v1 is a 2024 model with completely different multi-chip architecture. Flashing a 2018 WT60F168 binary onto an OPS v1 will permanently **brick** the hardware.
  2. **Tri-Mode Wireless & RF Dongle**: The OPS v1 communicates over 2.4GHz RF (`PID 0x4027`) and Bluetooth. Flashing a wired USB descriptor does not make the 2.4GHz adapter work and is irrelevant for Bluetooth.
  3. **Loss of Advanced Features**: Apple's `XboxGamepad.dext` has no support for OPS v1's RGB lighting zones, 3-way trigger locks, or rear paddles (AGL1–AGR2).
  4. **No Standalone Firmware ROM**: PowerA does not distribute unencrypted standalone ROM files for OPS v1.
- **Why User-Space IOKit Driver + Virtual HID Bridge is the Reliable & Feasible Path**:
  1. **100% Safe & Non-Destructive**: Zero firmware rewriting; no risk of bricking or voiding warranty.
  2. **Tri-Mode Support**: Handles Wired USB, 2.4GHz Wireless Dongle, and Bluetooth natively.
  3. **Full Feature Preservation**: Fully decodes all 20 bytes (including rear paddles) and provides bidirectional control over RGB lighting and dual-rumble grip motors.
  4. **Universal System Compatibility**: Through our virtual controller bridge (`tools/powera_karabiner_bridge.cpp`), it emits a standard HID gamepad that Apple Arcade, Steam, and emulators accept out of the box.

---

## 3. The macOS Solution: Native User-Space IOKit Pipe Reading

To solve this without needing a disabled kernel extension:
1. **Claim Interface 0 via IOKit User-Client**:
   `powera_driver.c` uses Apple's `IOUSBLib` (`IOUSBInterfaceInterface` via `kIOUSBInterfaceUserClientTypeID`) to locate `IOUSBHostInterface` where `bInterfaceNumber == 0`.
2. **Read Pipe 1 (Interrupt IN Synchronous Pipe)**:
   The driver calls `ReadPipe()` directly on Pipe 1.
   > [!IMPORTANT]
   > In macOS IOKit, calling `ReadPipeTO()` with non-zero timeout values on an Interrupt endpoint returns `kIOReturnBadArgument` (`0xE00002C2`) because the macOS kernel USB driver rejects non-zero timeout parameters on Interrupt endpoints. Calling synchronous `ReadPipe()` resolves this immediately and reliably decodes the 15/20-byte Xbox 360 packet at 500–1000Hz.
   > 
   > Furthermore, normal idle timeouts on interrupt endpoints in macOS IOKit can return `0xE00002ED` (`kIOReturnNotResponding`) or `0xE00002C2` (`kIOReturnBadArgument`). The driver distinguishes these temporary idle conditions from actual hardware disconnection (`kIOReturnNoDevice` `0xE00002C0` / `kIOReturnNotOpen` `0xE00002CD`), preventing false disconnect cycles and ensuring continuous telemetry reception.
3. **Turn On White Center LED & Inject Wake-Up Frames**:
   Upon opening Interface 0, the driver sends the solid white status LED command (`0x01 0x03 0x06`) and GIP power initialization frames (`0x05 0x20 0x00 0x01 0x00`, `0x0A 0x20 0x00 0x03 0x00 0x01 0x14`, `0x06 0x20 0x00 0x02 0x01 0x00`) on Pipe 2 (Interrupt OUT) to confirm an active data link.
4. **Feed UI Telemetry**:
   Decoded coordinates (`lx`, `ly`, `rx`, `ry`, `lt`, `rt`, `buttons`, `reports`, `rate`, `hex`) are served directly via `http://127.0.0.1:8765/api/status` and `http://127.0.0.1:8765/api/inputs` to the WebKit UI at 40Hz, bypassing the non-functional `navigator.getGamepads()`.
5. **System-wide Game Compatibility**:
   `powera_karabiner_bridge.cpp` consumes the driver telemetry and injects it into macOS via Karabiner VirtualHIDDevice, exposing a standard HID gamepad recognizable by Steam and Apple Arcade.

---

## 4. Universal Game Compatibility on macOS

On macOS, games and emulators rely on two primary input frameworks:
1. **SDL2 / SDL3 GameController Subsystem**: Used by Steam, OpenEmu, RetroArch, RPCS3, Ryujinx, Dolphin, PCSX2, and virtually all Mac ports (e.g. Baldur's Gate 3, Resident Evil 4, Death Stranding).
2. **Apple GameController.framework**: Used by Apple Arcade and native Mac App Store titles.

### Automated SDL GameController DB Configuration

The driver installs the following universal mapping strings:

```
# Wired USB (PID: 0x4025)
03000000d62000002540000000010000,PowerA OPS v1 Wired,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,

# 2.4GHz Dongle (PID: 0x4027 - PowerA OPS v1 Wireless Adapter)
03000000d62000002740000000010000,PowerA OPS v1 2.4G Wireless Adapter,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,

# 2.4GHz Dongle (PID: 0x400C)
03000000d62000000c40000000010000,PowerA OPS v1 2.4G Dongle,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,

# Bluetooth (PID: 0x4025)
05000000d62000002540000000010000,PowerA OPS v1 Bluetooth,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,

# Bluetooth (PID: 0x4027)
05000000d62000002740000000010000,PowerA OPS v1 Bluetooth,platform:Mac OS X,a:b0,b:b1,x:b2,y:b3,back:b4,start:b6,guide:b5,leftshoulder:b9,rightshoulder:b10,leftstick:b7,rightstick:b8,dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,paddle1:b11,paddle2:b12,paddle3:b13,paddle4:b14,
```

Mappings are synchronized into:
- `~/.config/gamecontrollerdb.txt`
- `SDL_GAMECONTROLLERCONFIG_FILE` environment variable in `~/.zshrc`

### Mac App Store & Apple Arcade Games (e.g. Asphalt 9: Legends)
Titles downloaded from the Mac App Store (such as *Asphalt 9: Legends*, *NBA 2K*, and Apple Arcade titles) do not use SDL; they exclusively query Apple's native **`GameController.framework` (`GCController`)**:
- **Why Asphalt 9 Shows "Keyboard Controls" Over Wired USB**:
  - Over a USB cable, the controller is an Xbox 360 Class `0xFF` device (`0x20D6:0x4025`).
  - Apple's DriverKit (`XboxGamepad.dext`) only recognizes Microsoft's Vendor ID (`0x045E`).
  - Because macOS does not publish a system `GCController` object for third-party USB controllers, Asphalt 9 sees no gamepad and defaults to keyboard controls.
- **The Native Solution (Bluetooth Mode)**:
  - Switching the bottom physical slider to **`BT`** switches the controller's radio to standard Bluetooth HID (`Usage: Gamepad`).
  - Apple's `GCController` instantly recognizes the controller, and Asphalt 9 automatically switches from keyboard to full native gamepad mode with analog steering, analog throttle/brake triggers, and on-screen button glyphs.
  - *Tip*: You can leave the USB-C cable plugged in to charge the battery while in `BT` mode.
- **Wired Alternative (Keyboard Emulation)**:
  - If you wish to stay connected via USB cable without Bluetooth, run the keyboard bridge (`bin/powera-karabiner-bridge`) to map controller buttons to Asphalt 9's keyboard controls (`A`/`D` steer, `Spacebar` nitro, `Shift` drift).

---

## 5. Background Daemon Service (`launchd`)

The driver runs as a background `launchd` daemon so that it is always active without needing the GUI app open.

### Service Commands:

- **One-Click Build & Install to System**:
  ```bash
  make install
  # or: ./scripts/install_driver.sh
  ```

- **Restart Daemon (One-Click Script)**:
  ```bash
  make restart
  # or: ./scripts/restart_driver.sh
  ```

- **Trigger Hardware Rescan via IPC**:
  ```bash
  curl -X POST http://127.0.0.1:8765/api/driver/rescan
  ```

- **Start Daemon via launchd**:
  ```bash
  launchctl load ~/Library/LaunchAgents/com.powera.driver.plist
  ```

- **Stop Daemon via launchd**:
  ```bash
  launchctl unload ~/Library/LaunchAgents/com.powera.driver.plist
  ```

- **Check Daemon Logs & Telemetry**:
  ```bash
  curl -s http://127.0.0.1:8765/api/status
  cat /tmp/powera_driver.log
  ```

---

## 6. Connection State Machine & Disconnect Lifecycle

The driver maintains strict real-time connection lifecycle tracking:
1. **Standby / Disconnected**: When no physical controller or 2.4G dongle is plugged into the USB bus, `hid_enumerate(0x20D6, 0x0)` returns `NULL`. The daemon serves `{"connected": false, "mode": 0, "battery": 0}` over IPC on port `8765`.
2. **Dynamic Hotplug Detection**: The driver input thread polls for device arrivals every 300ms. As soon as a PowerA device is plugged in (Wired USB `0x4025` or 2.4G Wireless Dongle `0x400C/0x4013`), it opens the device, discovers endpoint interfaces, activates the hardware status LED, and transitions to `connected: true`.
3. **Graceful Disconnect Cleanup**: If the USB cable or dongle is unplugged, the driver immediately releases file descriptors via `hid_close()`, zeroes out axis coordinates and trigger values, and transitions to `connected: false` with zero memory leaks.

---

## 7. Hardware Mode Switch & White LED Indicator Activation

### A. Physical 3-Position Slider Switch
The PowerA OPS v1 features a physical 3-position toggle switch on the bottom edge:
- **`USB` (Wired USB Mode)**: Required for direct cable gameplay and app configuration over wire.
- **`RF` (2.4 GHz Wireless Mode)**: Uses the USB wireless dongle. *Note: If plugged into USB while the switch is set to RF or BT, the controller only draws battery charging power; it does not establish a USB data link.*
- **`BT` (Bluetooth Mode)**: Uses standard Bluetooth pairing to macOS.

### B. Interface 0 Hardware White LED Packet
On Windows, Microsoft's `xusb22.sys` driver sends an Xbox 360 controller output report to turn on the center white status light. On macOS, our driver implements this natively:
- **Target Interface**: USB Interface 0 (`bInterfaceClass: 0xFF`, `bInterfaceSubClass: 0x5D`, `bInterfaceProtocol: 0x01`).
- **Endpoint**: Pipe 2 (Endpoint 1 OUT, Interrupt 64-byte).
- **Packet Structure**:
  - Byte 0: `0x01` (Command Type)
  - Byte 1: `0x03` (Subcommand Selector)
  - Byte 2: `0x06` (Solid White LED / Player 1 ON)
- **Automatic Execution**: The driver automatically issues this packet the moment the controller enumerates on macOS via USB, turning on the white status light to confirm an active connection.

### C. Troubleshooting Checklist: Why No Inputs/Motors Respond Over 2.4GHz Dongle
When the USB wireless adapter is plugged in, macOS will report `Connected (PID: 0x4027)` even if the handheld controller itself is turned off or un-synced.
If sticks, buttons, and motor tests produce no response:
1. **Wake Up Handheld Controller**: The OPS v1 goes into sleep mode after a few minutes of inactivity. Press and hold the center **PowerA Home/Shield** button to turn it on.
2. **Verify Slider Switch is in `RF` Mode**: On the bottom edge of the controller, ensure the 3-position slider is firmly in the **`RF`** (middle) position. If set to `USB` or `BT`, the controller will not transmit to the 2.4G dongle.
3. **Re-Sync Handheld Controller to Dongle**: If the LED on the controller is pulsing or flashing slowly, the RF link is disconnected:
   - Make sure the dongle is plugged into your Mac.
   - On the controller, press and hold the **top SYNC button** (next to the USB-C port) for **3 to 5 seconds** until the LED flashes **rapidly**.
   - The LED will turn solid, confirming the RF handshake is established.
4. **Wired Mode Alternative**: If wireless sync fails, connect the controller directly to your Mac using a USB-C data cable and switch the bottom slider to **`USB`**. Press Home, and raw USB packets flow immediately.

### D. Wired USB Power States: Why Windows Turns It On Automatically But macOS Left It In Standby
When connected via direct USB-C cable with the bottom switch set to **`USB`**:
- **Why Dual Motors Work Even When Inputs Do Not**:
  - The controller draws standard 5V bus power over USB.
  - USB Interface 0 Pipe 2 accepts interrupt OUT rumble packets regardless of whether the controller's button-scanning MCU is awake.
- **Why Windows Turns On The Controller Automatically**:
  - On Windows, Microsoft's `xusb22.sys` driver automatically sends an **XID `GET_CAPABILITIES` control transfer request** (`bmRequestType: 0xC1, bRequest: 0x01, wValue: 0x0100, wIndex: 0x0000, wLength: 20`) to Interface 0 during USB enumeration, followed by setting Player 1 solid LED (`0x01 0x03 0x02`).
  - The PowerA onboard firmware specifically waits for this exact host control request before waking up its microcontroller and turning on its front LED.
- **Why macOS Left It In Standby (Front LED OFF)**:
  - macOS has no built-in driver for Vendor ID `0x20D6` Class `0xFF` (Xbox 360 controller).
  - While the macOS kernel enumerates the physical USB device and provides 5V bus power, macOS **never issues the XID `GET_CAPABILITIES` control transfer**.
  - Without this handshake, the controller remains in low-power charging standby with its LED off, and the microcontroller refuses to power the button scan matrix or Hall-effect sensors (producing 0 reports on Pipe 1).
- **The Native Driver Solution**:
  - Our driver (`powera_driver.c`) claims Interface 0 via IOKit and immediately transmits the standard XID `GET_CAPABILITIES` control transfer (`0xC1, 0x01, 0x0100, 0x0000, 20`) and Player 1 Solid LED command (`0x01 0x03 0x02`).
  - The controller immediately wakes up, turns on its front center LED, and begins streaming live 20-byte reports over Pipe 1 at up to 1000Hz.

---

## 8. Multi-Stage Active Hardware Listener & Manual Input Mapping Wizard

### Feasibility of Manual Hardware Mapping
Manual input mapping on macOS without vendor SDKs is **fully feasible and highly reliable**. By diffing incoming hardware packets against an idle baseline at 500Hz, a dedicated differ tool can deterministically isolate the exact byte offset and bitmask for every button, bumper, trigger, paddle, and stick.

### Running the Multi-Stage Active Listener
The repository provides `bin/powera_active_listener`, compiled from [`tools/powera_active_listener.c`](../tools/powera_active_listener.c):

1. **Stage 1 (Direct IOKit USB Pipe Listener)**: Directly claims Pipe 1 (Interrupt IN) and Pipe 2 (Interrupt OUT) on USB Interface 0 (`0x20D6:0x4027` / `0x20D6:0x4025`).
2. **Stage 2 (HID / Config Reader)**: Listens on Interface 1 (`0xFF00`) or Bluetooth GATT endpoints.
3. **Stage 3 (Real-Time Bitwise Packet Differ)**: Visualizes incoming raw packets and highlights any modified bytes in ANSI yellow/green:
   ```bash
   ./bin/powera_active_listener
   ```
4. **Stage 4 (Interactive Input Mapping Wizard)**:
   Runs an interactive step-by-step mapping procedure that prompts the user sequentially through all inputs (A/B/X/Y, D-Pad, Bumpers, Triggers, Stick Clicks, Menu/View/Home, and Rear Paddles AGL1/AGL2/AGR1/AGR2):
   ```bash
   ./bin/powera_active_listener --map
   ```
   - Automatically unloads the background daemon (`com.powera.driver`) during the mapping session to acquire exclusive hardware access.
   - Detects the changed byte index and bitmask for each press.
   - Saves the final profile to `profiles/hardware_map.json`.
   - Restores the background daemon automatically upon exit.

### IPC Socket Optimization & Connection Stability
To prevent TCP socket starvation (`TIME_WAIT` pileup) caused by high-frequency UI status polling:
- The driver HTTP server enforces `Connection: close` and sets a 250ms socket timeout (`SO_RCVTIMEO` / `SO_SNDTIMEO`).
- The companion WebKit frontend (`src/ui/app.js`) implements an in-flight guard (`isPollingDaemon`) and throttles polling to a stable 20Hz (50ms), preventing socket saturation.

---

## 9. Rumble & Vibration Intensity Architecture

### Dual-Motor Mechanical Distinction: Heavy (Intense/Slow) vs. Light (Soft/Fast)
In standard gamepad engineering (Xbox / XInput specifications followed by PowerA), the two motors are **not stereo directional channels** (i.e. left vs. right events). Instead, they operate analogous to an audio crossover system:
- **Left Grip — Heavy Motor (Intense & Deep)**: Equipped with a large, heavy semicircular brass counterweight. Because of its large rotational inertia, it rotates at lower RPMs with substantial physical displacement, generating deep, slow, intense shaking (explosions, vehicle crashes, shotgun recoil).
- **Right Grip — Light Motor (Soft & Fast)**: Equipped with a small, lightweight counterweight. It spins at very high RPMs with low displacement, producing a crisp, soft, rapid buzz (subtle tactile textures, road gravel, UI clicks, laser hums).
- In modern games, both motors are blended simultaneously by the game engine to create rich, composite force-feedback textures.

### Protocol Implementation for 2.4GHz Dongle & Wired Modes
1. **Interface 0 Pipe 2 (Standard 8-Byte Xbox 360 Rumble Protocol)**:
   - On the 2.4GHz wireless dongle (`0x20D6:0x4027`) and wired USB (`0x20D6:0x4025`), the hardware firmware decodes rumble using the standard Xbox 360 Interrupt OUT 8-byte packet on Pipe 2:
     - Byte 0: `0x00` (Packet Type)
     - Byte 1: `0x08` (Length)
     - Byte 2: `0x00` (Subtype)
     - Byte 3: `left_grip` (Heavy low-frequency main motor magnitude: `0x00`–`0xFF` / `0`–`255`)
     - Byte 4: `right_grip` (Light high-frequency motor magnitude: `0x00`–`0xFF` / `0`–`255`)
     - Bytes 5–7: `0x00` (Padding)
   - Transmitting strictly this standard 8-byte packet prevents packet collisions that would otherwise overwrite motor speeds.

2. **Wireless RF Stop Delivery Guarantee & Generation Fencing**:
   - Because 2.4GHz RF wireless packets can experience transmission interference, calling `powera_driver_set_rumble(0, 0, 0, 0)` transmits repeated stop bursts spaced 5ms apart, ensuring 100% receipt by the wireless controller.
   - A thread-safe rumble generation token (`s_rumble_generation`) prevents race conditions between rapid consecutive slider movements and expiring pulse threads, guaranteeing that stop commands are executed cleanly.

3. **Wide-Range Dynamic Intensity Scaling**:
   - The driver provides full 0–255 direct linear intensity mapping from the UI sliders ($0\%–100\%$):
     $$\text{PWM}_{\text{left}} = \left\lfloor \frac{\text{lg\_pct} \times 255}{100} \right\rfloor, \quad \text{PWM}_{\text{right}} = \left\lfloor \frac{\text{rg\_pct} \times 255}{100} \right\rfloor$$
   - This unlocks the full dynamic range of both motors from a barely perceptible micro-tick at $1\%–5\%$ up to maximum physical vibration at $100\%$.

4. **Segmented HTTP POST Body Ingestion**:
   - WebKit (`fetch()`) in macOS Cocoa apps transmits HTTP request headers and JSON payloads in separate TCP segments.
   - The embedded driver IPC server evaluates `Content-Length` and buffers incoming socket data until the complete payload is assembled, preventing missing JSON parameters and defaulting errors.
   - Sliders in the Companion App UI automatically trigger live feedback pulses on release, and a dedicated **Stop All Motors** button provides immediate manual override.

---

## 8. Active Hardware Input Tester

The companion app includes a dedicated, zero-latency **Active Hardware Input Tester** tab (`#tab-tester`):

1. **Digital Button Matrix**:
   - Real-time visual feedback for all face buttons (A in Emerald Green, B in Crimson Red, X in Cobalt Blue, Y in Amber Gold).
   - D-Pad directions (Up, Down, Left, Right).
   - Bumpers (LB, RB), Stick Clicks (LSB/L3, RSB/R3), and System buttons (View, Menu, Guide/Home).
   - Dedicated testing badges for all 4 Advanced Gaming Buttons (AGL1, AGL2, AGR1, AGR2 in Royal Purple).

2. **Precision Analog Thumbstick Radars**:
   - Dual 60fps HTML5 Canvas radar scopes rendering live stick deflection vectors, concentric guideline rings, crosshairs, and glowing position indicators.
   - Live Cartesian coordinate readouts: `X: [-1.000, +1.000] | Y: [-1.000, +1.000]`.
   - Polar coordinate resolution: Deflection Magnitude `[0.00, 1.00]` and Angular Heading in degrees (`0°–360°`).

3. **Analog Trigger Travel Gauges**:
   - High-resolution vertical fill meters tracking analog trigger depression from `0%` to `100%`.
   - Emulated digital trigger click badges that light up when crossing the tactile threshold (`raw > 30`).

4. **Telemetry Header Chips & Live 15-Byte Hex Stream**:
   - **Rate**: Live hardware polling rate in Hz calculated continuously over rolling 500ms intervals (125Hz–1000Hz).
   - **Packets**: Cumulative packet counter incremented on every Interrupt IN report.
   - **Link Mode**: Active transport link identifier (`2.4G RF Link`, `USB-C Direct`, or `Bluetooth LE`).
   - **Live 15-Byte Stream**: Monospace hex telemetry inspector displaying the raw packets received directly from the controller hardware (`00 14 d1 d2 lt rt lx_lo lx_hi ly_lo ly_hi rx_lo rx_hi ry_lo ry_hi paddles`).


