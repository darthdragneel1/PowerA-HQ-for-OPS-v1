# PowerA OPS v1 - Karabiner-Elements Integration Guide

This guide explains how to use the lightweight **PowerA Karabiner-Elements Bridge** (available in both Python and high-performance native C++) to inject controller inputs (buttons, sticks, triggers, and back paddles) directly into macOS applications, games, and the desktop environment.

---

## 1. Overview

While standard gamepads are handled by `IOHIDFamily` or `SDL2`, many workflows (desktop navigation, emulators, macro key bindings, productivity apps, or games without native gamepad support) require controller inputs to be translated into **system-wide keyboard strokes, mouse movements, or Karabiner virtual events**.

The PowerA Karabiner Bridge provides two solutions:
1. **Automated Karabiner-Elements Complex Modifications**: Generates native JSON rules for Karabiner-Elements under `~/.config/karabiner/assets/complex_modifications/powera_ops_v1.json`.
2. **Real-Time Input Injection Daemon**: Directly captures incoming PowerA USB / 2.4GHz Dongle / Bluetooth HID reports and posts native events to the macOS Window Server via `CGEvent` and Karabiner virtual endpoints.

---

## 2. Using the Native C++ Bridge

The C++ bridge binary is compiled to:
```bash
./bin/powera-karabiner-bridge
```

### Install Karabiner Rules
```bash
./bin/powera-karabiner-bridge --install-karabiner-rules
```
This generates:
```
~/.config/karabiner/assets/complex_modifications/powera_ops_v1.json
```
You can then open **Karabiner-Elements -> Complex Modifications -> Add rule** and enable the PowerA OPS v1 mapping.

### Run in Background with Mouse Emulation
To control the macOS mouse cursor with the Right Analog Stick, with Right Trigger (RT) as Left Click and Left Trigger (LT) as Right Click:
```bash
./bin/powera-karabiner-bridge --mouse-mode --rate 500
```

---

## 3. Using the Python Bridge

A clean Python implementation is provided at:
```bash
tools/powera_karabiner_bridge.py
```

### Requirements:
```bash
pip install hidapi pyobjc-framework-Quartz
```

### Execution:
```bash
python3 tools/powera_karabiner_bridge.py --install-karabiner-rules --mouse-mode
```

---

## 4. Default Key & Paddle Mapping Table

| Controller Button | macOS Action / Key | Function |
|---|---|---|
| **AGL1 (Upper Left Paddle)** | `W` (Keycode 13) | Forward movement / Primary action |
| **AGL2 (Lower Left Paddle)** | `S` (Keycode 1) | Backward movement / Secondary action |
| **AGR1 (Upper Right Paddle)** | `Spacebar` (Keycode 49) | Jump / Confirm |
| **AGR2 (Lower Right Paddle)** | `Left Shift` (Keycode 56) | Sprint / Modifier |
| **Button A** | `Spacebar` | Confirm / Primary |
| **Button B** | `C` / `Escape` | Cancel / Crouch |
| **Button X** | `R` (Keycode 15) | Reload / Action |
| **Button Y** | `E` (Keycode 14) | Interact |
| **Left Bumper (LB)** | `Q` (Keycode 12) | Ability 1 |
| **Right Bumper (RB)** | `Tab` (Keycode 48) | Ability 2 / Scoreboard |
| **Right Stick (Mouse Mode)** | Mouse Pointer (X/Y) | Smooth cursor aiming |
| **Right Trigger (RT)** | Mouse Left Click | Fire / Select |
| **Left Trigger (LT)** | Mouse Right Click | Aim Down Sights / Context Menu |
| **D-Pad Up / Down / Left / Right** | Arrow Keys | Directional navigation |
| **View (Back)** | `Escape` | Menu / Back |
| **Menu (Start)** | `Return` | Pause / Enter |

Keycodes in `powera_karabiner_bridge.py` or `powera_karabiner_bridge.cpp` can be freely edited to match any specific game or productivity workflow.
