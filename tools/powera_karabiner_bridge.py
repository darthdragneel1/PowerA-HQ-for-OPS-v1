#!/usr/bin/env python3
"""
PowerA OPS v1 - Karabiner-Elements & macOS Input Injection Bridge
Lightweight, open-source background tool to read PowerA OPS v1 controller inputs
(Wired USB, 2.4GHz Dongle, or Bluetooth) and inject them into macOS via
Karabiner-Elements Virtual HID Device and Quartz/CGEvent.

Usage:
    python3 powera_karabiner_bridge.py [--install-karabiner-rules] [--mouse-mode] [--rate 125|250|500|1000]
"""

import sys
import os
import time
import json
import argparse
from pathlib import Path

# Try importing hid (hidapi)
try:
    import hid  # type: ignore
except ImportError:
    print("[PowerA Bridge] Warning: 'hid' library not found. Run: pip install hidapi")
    hid = None

# macOS Native Quartz Event Injection fallback (CoreGraphics)
try:
    import Quartz  # type: ignore
    from Quartz import (  # type: ignore
        CGEventCreate,
        CGEventGetLocation,
        CGEventCreateKeyboardEvent,
        CGEventCreateMouseEvent,
        CGEventPost,
        kCGHIDEventTap,
        kCGEventKeyDown,
        kCGEventKeyUp,
        kCGEventMouseMoved,
        kCGEventLeftMouseDown,
        kCGEventLeftMouseUp,
        kCGEventRightMouseDown,
        kCGEventRightMouseUp,
    )
    HAS_QUARTZ = True
except ImportError:
    HAS_QUARTZ = False

# USB Vendor and Product IDs
POWERA_VID = 0x20D6
POWERA_PIDS = {
    0x4027: "PowerA OPS v1 Wireless Controller - Adapter (2.4GHz Dongle)",
    0x4025: "PowerA OPS v1 Wireless Controller (Wired)",
    0x401B: "PowerA OPS v1 Fortnite Edition",
    0x4032: "PowerA OPS v3 Pro Wireless Controller",
    0x400C: "PowerA 2.4GHz Wireless Dongle (OPS v1 alternate)",
    0x4013: "PowerA 2.4GHz Wireless Dongle alt",
    0x401E: "PowerA 2.4GHz Wireless Dongle (OPS v3)",
    0x4022: "PowerA 2.4GHz Wireless Dongle alt 2",
    0x404F: "PowerA Battle Dragon Dongle"
}

# Key mapping dictionary (macOS virtual keycodes)
# Can be customized by user or configured via command line
DEFAULT_KEYMAP = {
    # Face Buttons
    "A": 49,      # Spacebar
    "B": 53,      # Escape
    "X": 12,      # Q
    "Y": 14,      # E
    
    # Bumpers & Triggers
    "LB": 0,      # A (Strafe Left)
    "RB": 2,      # D (Strafe Right)
    "LT": 48,     # Tab
    "RT": 36,     # Return / Enter
    
    # Menu & View
    "VIEW": 51,   # Backspace
    "MENU": 53,   # Escape
    "GUIDE": 122, # F1
    
    # D-Pad
    "DPAD_UP": 126,    # Up Arrow
    "DPAD_DOWN": 125,  # Down Arrow
    "DPAD_LEFT": 123,  # Left Arrow
    "DPAD_RIGHT": 124, # Right Arrow
    
    # Thumbstick Clicks
    "LSB": 56,    # Left Shift (Sprint)
    "RSB": 8,     # C (Crouch)
    
    # Advanced Gaming Buttons (Back Paddles)
    "AGL1": 13,   # W (Forward)
    "AGL2": 1,    # S (Backward)
    "AGR1": 49,   # Space (Jump)
    "AGR2": 56    # Left Shift
}

def install_karabiner_complex_modifications():
    """Generates and installs Karabiner-Elements Complex Modification rules."""
    karabiner_dir = Path.home() / ".config" / "karabiner" / "assets" / "complex_modifications"
    karabiner_dir.mkdir(parents=True, exist_ok=True)
    rule_file = karabiner_dir / "powera_ops_v1.json"

    rule_data = {
        "title": "PowerA OPS v1 Controller - macOS Karabiner Mappings",
        "rules": [
            {
                "description": "PowerA OPS v1: Map Advanced Gaming Back Paddles (AGL1, AGL2, AGR1, AGR2)",
                "manipulators": [
                    {
                        "type": "basic",
                        "from": {
                            "pointing_button": "button4",
                            "modifiers": { "optional": ["any"] }
                        },
                        "to": [ { "key_code": "left_shift" } ]
                    },
                    {
                        "type": "basic",
                        "from": {
                            "pointing_button": "button5",
                            "modifiers": { "optional": ["any"] }
                        },
                        "to": [ { "key_code": "spacebar" } ]
                    }
                ]
            },
            {
                "description": "PowerA OPS v1: Universal Gamepad Enhancement for macOS",
                "manipulators": [
                    {
                        "type": "basic",
                        "from": {
                            "key_code": "f13",
                            "modifiers": { "optional": ["any"] }
                        },
                        "to": [ { "consumer_key_code": "play_or_pause" } ]
                    }
                ]
            }
        ]
    }

    with open(rule_file, "w") as f:
        json.dump(rule_data, f, indent=2)

    print(f"[PowerA Bridge] Successfully installed Karabiner-Elements rules into:\n  {rule_file}")
    print("[PowerA Bridge] You can enable this rule in Karabiner-Elements -> Complex Modifications -> Add rule.")

class PowerAKarabinerBridge:
    def __init__(self, mouse_mode=False, poll_hz=250):
        self.mouse_mode = mouse_mode
        self.poll_hz = poll_hz
        self.dev = None
        self.connected_pid = None
        self.btn_prev = 0

    def connect(self):
        """Finds and connects to PowerA controller via USB / Dongle / BT."""
        if not hid:
            print("[PowerA Bridge] hidapi module not installed.")
            return False

        for pid, name in POWERA_PIDS.items():
            try:
                dev = hid.device()
                dev.open(POWERA_VID, pid)
                self.dev = dev
                self.connected_pid = pid
                print(f"[PowerA Bridge] Connected to: {name} (VID: 0x{POWERA_VID:04X}, PID: 0x{pid:04X})")
                return True
            except Exception:
                continue

        # Try opening via enumeration
        for d in hid.enumerate(POWERA_VID, 0):
            try:
                dev = hid.device()
                dev.open_path(d['path'])
                self.dev = dev
                self.connected_pid = d['product_id']
                path_str = d['path'].decode('latin1', errors='replace') if isinstance(d['path'], bytes) else str(d['path'])
                print(f"[PowerA Bridge] Connected to device at path: {path_str}")
                return True
            except Exception:
                continue

        return False

    def send_key(self, keycode, is_press):
        """Injects a keyboard key event via macOS Quartz."""
        if not HAS_QUARTZ:
            return
        event = CGEventCreateKeyboardEvent(None, keycode, is_press)
        CGEventPost(kCGHIDEventTap, event)

    def send_mouse_move(self, dx, dy):
        """Injects relative mouse motion via macOS Quartz."""
        if not HAS_QUARTZ:
            return
        try:
            our_event = CGEventCreate(None)
            cur_pt = CGEventGetLocation(our_event)
            new_pt = (cur_pt.x + dx, cur_pt.y + dy)
            evt = CGEventCreateMouseEvent(None, kCGEventMouseMoved, new_pt, 0)
            CGEventPost(kCGHIDEventTap, evt)
        except Exception:
            pass

    def send_mouse_click(self, btn, is_press):
        """Injects mouse click (0=left, 1=right)."""
        if not HAS_QUARTZ:
            return
        try:
            our_event = CGEventCreate(None)
            cur_pt = CGEventGetLocation(our_event)
            if btn == 0:
                evt_type = kCGEventLeftMouseDown if is_press else kCGEventLeftMouseUp
            else:
                evt_type = kCGEventRightMouseDown if is_press else kCGEventRightMouseUp
            evt = CGEventCreateMouseEvent(None, evt_type, cur_pt, btn)
            CGEventPost(kCGHIDEventTap, evt)
        except Exception:
            pass

    def run(self):
        print(f"[PowerA Bridge] Starting background input service ({self.poll_hz} Hz)...")
        print("[PowerA Bridge] Supported modes: Wired USB, 2.4GHz Dongle, and Bluetooth.")
        print("[PowerA Bridge] Press Ctrl+C to terminate.")

        delay = 1.0 / self.poll_hz
        while True:
            if not self.dev:
                if not self.connect():
                    time.sleep(1.0)
                    continue

            try:
                # Read 64-byte HID input report
                data = self.dev.read(64, timeout_ms=20)
                if not data:
                    continue

                self.process_report(bytes(data))
                time.sleep(delay)
            except Exception as e:
                print(f"[PowerA Bridge] Connection lost: {e}")
                self.dev = None
                time.sleep(1.0)

    def process_report(self, report):
        """Parses report bytes and injects corresponding macOS inputs."""
        if len(report) < 8:
            return

        # Typical input report:
        # byte 0: report ID
        # byte 1..4: stick X, Y, Z, Rz
        # Axes & Triggers
        _lx = report[1] if len(report) > 1 else 128
        _ly = report[2] if len(report) > 2 else 128
        rx = report[3] if len(report) > 3 else 128
        ry = report[4] if len(report) > 4 else 128
        _lt = report[5] if len(report) > 5 else 0
        _rt = report[6] if len(report) > 6 else 0

        # Buttons bitmask (including 32-bit extended back paddles)
        buttons = (report[7] | (report[8] << 8)) if len(report) > 8 else 0
        if len(report) >= 11:
            buttons |= (report[9] << 16) | (report[10] << 24)

        # Mouse mode: Map right stick to mouse pointer, RT to left click, LT to right click
        if self.mouse_mode:
            dx = (rx - 128) / 12.0
            dy = (ry - 128) / 12.0
            if abs(dx) > 1.2 or abs(dy) > 1.2:
                self.send_mouse_move(dx, dy)

            # RT click
            if (buttons & (1 << 7)) and not (self.btn_prev & (1 << 7)):
                self.send_mouse_click(0, True)
            elif not (buttons & (1 << 7)) and (self.btn_prev & (1 << 7)):
                self.send_mouse_click(0, False)

            # LT right click
            if (buttons & (1 << 6)) and not (self.btn_prev & (1 << 6)):
                self.send_mouse_click(1, True)
            elif not (buttons & (1 << 6)) and (self.btn_prev & (1 << 6)):
                self.send_mouse_click(1, False)

        # Standard button event injection
        btn_defs = [
            ("A", 1 << 0), ("B", 1 << 1), ("X", 1 << 2), ("Y", 1 << 3),
            ("LB", 1 << 4), ("RB", 1 << 5), ("VIEW", 1 << 8), ("MENU", 1 << 9),
            ("LSB", 1 << 10), ("RSB", 1 << 11),
            ("DPAD_UP", 1 << 12), ("DPAD_DOWN", 1 << 13),
            ("DPAD_LEFT", 1 << 14), ("DPAD_RIGHT", 1 << 15),
            ("AGL1", 1 << 16), ("AGL2", 1 << 17),
            ("AGR1", 1 << 18), ("AGR2", 1 << 19)
        ]

        for name, mask in btn_defs:
            is_down = bool(buttons & mask)
            was_down = bool(self.btn_prev & mask)
            if is_down != was_down and name in DEFAULT_KEYMAP:
                keycode = DEFAULT_KEYMAP[name]
                self.send_key(keycode, is_down)

        self.btn_prev = buttons

def main():
    parser = argparse.ArgumentParser(description="PowerA OPS v1 Karabiner-Elements & macOS Input Bridge")
    parser.add_argument("--install-karabiner-rules", action="store_true", help="Install Karabiner-Elements complex modification rules")
    parser.add_argument("--mouse-mode", action="store_true", help="Enable mouse cursor control on right analog stick")
    parser.add_argument("--rate", type=int, default=250, choices=[125, 250, 500, 1000], help="Input report polling rate (Hz)")
    args = parser.parse_args()

    if args.install_karabiner_rules:
        install_karabiner_complex_modifications()
        if len(sys.argv) == 2:
            return

    bridge = PowerAKarabinerBridge(mouse_mode=args.mouse_mode, poll_hz=args.rate)
    bridge.run()

if __name__ == "__main__":
    main()
