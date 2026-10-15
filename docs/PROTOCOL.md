# PowerA OPS v1 Hardware Protocol & Packet Specification

This document details the reverse-engineered hardware communication protocol used by the **PowerA OPS v1** and **Battle Dragon** controller family, disassembled from `PowerA_PC_HQ.exe` (v1.4.2-1).

---

## 1. Packet Framing

All configuration commands are sent as 64-byte USB HID Feature or Interrupt OUT reports starting with magic byte `0xE1`.

### 64-Byte Packet Layout:

| Offset | Size (Bytes) | Field Name | Description |
|---|---|---|---|
| `0x00` | 1 | Magic Byte | Fixed `0xE1` |
| `0x01` | 1 | Protocol Flag | Fixed `0x00` |
| `0x02` | 1 | Channel ID | Fixed `0x01` |
| `0x03` | 1 | Payload Length | Fixed `0x3C` (60 bytes) |
| `0x04` | 1 | Command Opcode | See Opcode Table below |
| `0x05` | 1 | Subcommand / Index | Subcommand or chunk index |
| `0x06..0x3D` | 56 | Payload Data | Command arguments / profile data |
| `0x3E` | 1 | CRC High Byte | High 8 bits of 16-bit CRC |
| `0x3F` | 1 | CRC Low Byte | Low 8 bits of 16-bit CRC |

---

## 2. Proprietary 16-Bit CRC Algorithm

Decompiled from x86 function at `0x45DB00`:

```c
void powera_calculate_crc(const uint8_t *buf, size_t start, size_t end, uint8_t *crc_hi, uint8_t *crc_lo) {
    uint16_t cx = 0;
    for (size_t i = start; i < end; i++) {
        uint8_t byte_val = buf[i];
        uint16_t edx = cx;
        uint32_t eax = (byte_val ^ edx) & 0xFF;
        edx = (edx >> 8) & 0xFF;
        
        uint32_t ebx = eax & 0x0F;
        uint32_t ecx_val = eax;
        
        eax = (ebx << 4) ^ ecx_val;
        ecx_val = eax;
        eax = ((eax << 1) ^ (ecx_val >> 4)) & 0xFF;
        ecx_val = ((ecx_val >> 4) ^ edx) & 0xFF;
        
        eax = (eax << 4) & 0xFF;
        ebx = (ebx ^ eax) & 0xFF;
        ebx = (ebx << 3) & 0xFFFF;
        
        cx = (uint16_t)((ecx_val ^ ebx) & 0xFFFF);
    }
    if (crc_hi) *crc_hi = (uint8_t)((cx >> 8) & 0xFF);
    if (crc_lo) *crc_lo = (uint8_t)(cx & 0xFF);
}
```

The CRC covers bytes `0x04` through `0x3D` (indices 4 to 62).

---

## 3. Command Opcodes

| Opcode | Function | Disassembly Caller |
|---|---|---|
| `0x20` | **4-Motor Rumble Control** | `0x40C5F4 - 0x40C63B` |
| `0xAD` | **Read Profile Chunks (0..6)** | `0x45E089`, `0x45E0DE`, `0x45E12E` |
| `0xAE` | **Write Profile Chunks (0..6)** | `0x45E1E8`, `0x45E236`, `0x45E286` |
| `0xA2` | **Stick Deadzones & Curves** | `0x45DD98` |
| `0xB5` | **Trigger Stop / Deadzones** | `0x45DDF8` |
| `0xA6` | **RGB Lighting Modes** | `0x45DEB8` |
| `0xA8` | **RGB Custom Color** | `0x45DF78` |
| `0xAA` | **RGB Speed & Brightness** | `0x45E038` |
| `0xB1` | **Calibration Start** | `0x45E330` |
| `0xB2` | **Calibration Step** | `0x45E3C8` |
| `0xB3` | **Calibration Finish** | `0x45E378` |
| `0xC2` | **Dongle Pair / Sync Status** | `0x45F119` |
| `0xF1` | **Query Firmware Version** | `0x45E4E8` |
| `0xFB` | **Query Battery Status** | `0x45E538` |

### Rumble Hardware Architecture (Dual Grip Motors)

The PowerA OPS v1 physically features two independent vibration motors in the grips:
1. **Left Main Motor** (`左马达`): Low-frequency heavy grip rotor (eccentric brass weight for impacts/explosions).
2. **Right Main Motor** (`右马达`): High-frequency crisp grip rotor (lightweight rotor for subtle feedback).
*(Note: Impulse trigger rumble motors are physically omitted on the OPS v1, and are exclusive to higher-tier controllers like OPS v3 Pro. On OPS v1, trigger impulse fields are set to 0)*.

Packet layout:
```c
uint8_t rumble_pkt[15] = {
    0x20, // Opcode
    0x09, // Duty/pulse parameter
    0x00, 
    0x64, // Frequency
    0x09, 
    0x00, 
    0x0F, 
    0x32, 
    0x32, 
    left_grip,      // 0 - 255
    right_grip,     // 0 - 255
    left_trigger,   // 0 - 255
    right_trigger,  // 0 - 255
    0x00, 
    0xFF  // Enable mask
};
```

### USB Interface 0 Pipe 2 Rumble Packet Formats

In addition to vendor command `0x20`, hardware rumble on Interface 0 (Interrupt OUT Pipe 2) is driven via standard XInput/Xbox 360 descriptors:

1. **Standard Wired Xbox 360 Format (8 Bytes)**:
```c
uint8_t x360_rumble[8] = {
    0x00,               // Message Type
    0x08,               // Payload Length
    0x00,               // Reserved
    left_grip_pwm,      // Heavy low-frequency motor magnitude (0 - 255)
    right_grip_pwm,     // Light high-frequency motor magnitude (0 - 255)
    0x00, 0x00, 0x00    // Padding
};
```

2. **Wireless Xbox 360 Receiver Format (12 Bytes)**:
```c
uint8_t w360_rumble[12] = {
    0x00, 0x01, 0x0F, 0xC0, 0x00, // Wireless adapter command header
    left_grip_pwm,                // Left motor magnitude (0 - 255)
    right_grip_pwm,               // Right motor magnitude (0 - 255)
    0x00, 0x00, 0x00, 0x00, 0x00  // Padding
};
```
- **Stop Command**: Sending `left_grip_pwm = 0` and `right_grip_pwm = 0` terminates rumble. The driver repeats the stop sequence across multiple bursts spaced 4ms apart to ensure RF reception over 2.4GHz wireless links.

---

## 4. Binary Profile File Format (`.pwa`)

Authentic `.pwa` profile files are exactly **173 bytes**.

### Memory Layout:

| Offset | Length | Description |
|---|---|---|
| `0x00..0x0B` | 12 bytes | Button Mappings: `[A, B, X, Y, LB, RB, AGL1, AGR1, AGL2, AGR2, View, Menu]` |
| `0x0C..0x13` | 8 bytes | Calibration padding (`0x40`) |
| `0x14` | 1 byte | Left Stick Inner Deadzone (0 - 100%) |
| `0x17` | 1 byte | Left Stick Outer Deadzone |
| `0x19` | 1 byte | Left Stick Curve (`0: Linear, 1: Smooth, 2: Aggressive, 3: Instant`) |
| `0x1E` | 1 byte | Right Stick Inner Deadzone |
| `0x21` | 1 byte | Right Stick Outer Deadzone |
| `0x23` | 1 byte | Right Stick Curve |
| `0x32` | 1 byte | Left Trigger End Stop (`99 for T1, 50 for T2, 0 for T3`) |
| `0x34` | 1 byte | Right Trigger End Stop (`99 for T1, 50 for T2, 0 for T3`) |
| `0x36` | 1 byte | Left Trigger Start Deadzone |
| `0x38` | 1 byte | Right Trigger Start Deadzone |
| `0x42` | 1 byte | Left Grip Motor Strength (0 - 100%) |
| `0x43` | 1 byte | Right Grip Motor Strength |
| `0x44` | 1 byte | Left Trigger Impulse Strength |
| `0x45` | 1 byte | Right Trigger Impulse Strength |
| `0x7E` | 1 byte | RGB Mode (`0: Static, 1: Breathing, 2: Cycle, 3: Rainbow, 4: Flame`) |
| `0x7F` | 1 byte | RGB Zone (`0: All, 1: Head, 2: Flame, 3: Body`) |
| `0x80` | 1 byte | RGB Direction (`0: Left-to-Right, 1: Right-to-Left, etc.`) |
| `0x82` | 1 byte | RGB Brightness (0 - 100%) |
| `0x83` | 1 byte | RGB Speed (0 - 100%) |
| `0x84..0x86` | 3 bytes | RGB Primary Color `[R, G, B]` |

---

## 5. Device Identification & USB Product IDs

| Product ID (PID) | Device Name | Connection Mode |
|---|---|---|
| `0x4025` | PowerA OPS v1 Wireless Controller | **Wired USB** (`Mode 1`) |
| `0x4027` | PowerA OPS v1 Wireless Controller - Adapter | **2.4GHz Wireless Dongle** (`Mode 2`) |
| `0x400C` | PowerA OPS v1 2.4GHz Wireless Dongle (Rev A) | **2.4GHz Wireless Dongle** (`Mode 2`) |
| `0x4013` | PowerA OPS v1 2.4GHz Wireless Dongle (Rev B) | **2.4GHz Wireless Dongle** (`Mode 2`) |
| `0x401B` | PowerA OPS v1 Fortnite Edition | **Wired USB** (`Mode 1`) |
| `0x4032` | PowerA OPS v3 Pro Wireless Controller | **Wired USB** (`Mode 1`) |
| `0x401E` | PowerA OPS v3 Pro 2.4GHz Wireless Dongle | **2.4GHz Wireless Dongle** (`Mode 2`) |
| `0x4022` | PowerA OPS v3 Pro 2.4GHz Wireless Dongle (Alt) | **2.4GHz Wireless Dongle** (`Mode 2`) |

---

## 6. Live Controller Input Stream & Packet Structure (Interface 0 Interrupt IN)

On USB Interface 0 (Class `0xFF`, Subclass `0x5D`, Protocol `0x01`), the controller and 2.4GHz wireless adapter stream live user inputs across **Interrupt IN Pipe 1**.

### 15-Byte Live Input Report Layout:

| Byte Offset | Field | Type | Description / Bitmask |
|---|---|---|---|
| `0x00` | Packet Type | `uint8_t` | Fixed `0x00` |
| `0x01` | Packet Length | `uint8_t` | Fixed `0x14` (20 bytes allocated descriptor length) |
| `0x02` | Digital Buttons 1 (`d1`) | `uint8_t` | Bit 0: D-Pad Up (`0x01`)<br/>Bit 1: D-Pad Down (`0x02`)<br/>Bit 2: D-Pad Left (`0x04`)<br/>Bit 3: D-Pad Right (`0x08`)<br/>Bit 4: Menu / Start (`0x10`)<br/>Bit 5: View / Back (`0x20`)<br/>Bit 6: LSB / L3 Stick Click (`0x40`)<br/>Bit 7: RSB / R3 Stick Click (`0x80`) |
| `0x03` | Digital Buttons 2 (`d2`) | `uint8_t` | Bit 0: LB / Left Bumper (`0x01`)<br/>Bit 1: RB / Right Bumper (`0x02`)<br/>Bit 2: Guide / Xbox Home (`0x04`)<br/>Bit 4: Face Button A (`0x10`)<br/>Bit 5: Face Button B (`0x20`)<br/>Bit 6: Face Button X (`0x40`)<br/>Bit 7: Face Button Y (`0x80`) |
| `0x04` | Left Trigger (LT) | `uint8_t` | Analog travel magnitude: `0` (released) to `255` (fully depressed). Threshold `> 30` registers as digital trigger click. |
| `0x05` | Right Trigger (RT) | `uint8_t` | Analog travel magnitude: `0` (released) to `255` (fully depressed). Threshold `> 30` registers as digital trigger click. |
| `0x06..0x07` | Left Stick X | `int16_t` | Little-endian signed 16-bit integer (`-32768` full left to `+32767` full right). |
| `0x08..0x09` | Left Stick Y | `int16_t` | Little-endian signed 16-bit integer (`-32768` full down to `+32767` full up). |
| `0x0A..0x0B` | Right Stick X | `int16_t` | Little-endian signed 16-bit integer (`-32768` full left to `+32767` full right). |
| `0x0C..0x0D` | Right Stick Y | `int16_t` | Little-endian signed 16-bit integer (`-32768` full down to `+32767` full up). |
| `0x0E` | Back Paddles (AGL/AGR) | `uint8_t` | **PowerA Proprietary Hardware Extension Byte**:<br/>Bit 0 (`0x01`): **AGL1** (Left Upper Grip Paddle)<br/>Bit 1 (`0x02`): **AGL2** (Left Lower Grip Paddle)<br/>Bit 2 (`0x04`): **AGR1** (Right Upper Grip Paddle)<br/>Bit 3 (`0x08`): **AGR2** (Right Lower Grip Paddle) |

### Hardware Streaming States & Wake-Up Handshake (Control Pipe 0 & Pipe 2)
- **Standby Mode vs Active Streaming Mode**:
  - In Wired USB mode with 5V bus power applied, the controller MCU remains in **Hardware Standby** if the center front LED is **OFF**. In standby mode, the button scan matrix and stick ADCs remain unpowered, resulting in 0 packets on Pipe 1.
  - Windows automatically wakes the controller up via Microsoft's `xusb22.sys` during USB enumeration by issuing an **XID `GET_CAPABILITIES` control transfer**.
  - macOS has no built-in XID driver, so our user-space driver implements this exact handshake.
- **XID `GET_CAPABILITIES` Control Transfer (Endpoint 0)**:
  - `bmRequestType`: `0xC1` (Device-to-Host, Vendor-specific, Interface recipient)
  - `bRequest`: `0x01` (`GET_CAPABILITIES`)
  - `wValue`: `0x0100` (Input capabilities)
  - `wIndex`: `0x0000` (Interface 0)
  - `wLength`: `20` (20 bytes returned: `00 14 FF F7 FF FF C0 FF C0 FF C0 FF C0 FF 00 00 00 00 00 00`)
- **Pipe 2 Initialization Packets (Interrupt OUT)**:
  - Player 1 Solid White LED: `0x01 0x03 0x02`
  - GIP Power Wakeup: `0x05 0x20 0x00 0x01 0x00`
  - GIP LED Activation: `0x0A 0x20 0x00 0x03 0x00 0x01 0x14`
  - GIP Auth Handshake: `0x06 0x20 0x00 0x02 0x01 0x00`

---

## 7. Local Driver IPC REST Endpoints

The native daemon exposes a high-speed local HTTP/JSON server on `http://127.0.0.1:8765`:

- **`GET /api/status`**: Complete controller telemetry, connection state, battery percentage, active profile ID, report rate (Hz), cumulative packet count, and raw 15-byte hex string.
- **`GET /api/inputs`**: Minimalist ultra-low latency input snapshot for high-speed client loops.
- **`POST /api/rumble`**: Real-time vibration dispatch (`{"lg": 0..100, "rg": 0..100, "duration": 350}`).
- **`POST /api/rgb`**: Lighting mode, zone, and RGB color update.
- **`POST /api/profile`**: Onboard memory profile save / synchronization.
- **`POST /api/driver/rescan`**: Triggers immediate USB interface reconnection scan.
