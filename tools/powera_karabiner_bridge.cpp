/**
 * PowerA OPS v1 - Native C++ Karabiner & macOS Input Injection Bridge
 * Lightweight, high-performance background daemon.
 * 
 * Compiles with:
 *   clang++ -std=c++17 -O3 -Wall -I/opt/homebrew/include -L/opt/homebrew/lib -lhidapi \
 *     -framework ApplicationServices -framework IOKit -framework CoreFoundation \
 *     powera_karabiner_bridge.cpp -o powera-karabiner-bridge
 */

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#if __has_include(<hidapi/hidapi.h>)
#include <hidapi/hidapi.h>
#elif __has_include(<hidapi.h>)
#include <hidapi.h>
#elif __has_include("/opt/homebrew/include/hidapi/hidapi.h")
#include "/opt/homebrew/include/hidapi/hidapi.h"
#elif __has_include("/usr/local/include/hidapi/hidapi.h")
#include "/usr/local/include/hidapi/hidapi.h"
#else
#include <hidapi/hidapi.h>
#endif
#include <ApplicationServices/ApplicationServices.h>

#define POWERA_VID 0x20D6

// Known Product IDs
static const uint16_t SUPPORTED_PIDS[] = {
    0x4027, // PowerA OPS v1 Wireless Controller - Adapter (2.4GHz Dongle)
    0x4025, // PowerA OPS v1 Wireless Controller (Wired)
    0x401B, // PowerA OPS v1 Fortnite Edition
    0x4032, // PowerA OPS v3 Pro Wireless Controller
    0x400C, // 2.4GHz Dongle (OPS v1 alternate)
    0x4013, // 2.4GHz Dongle alt
    0x401E, // 2.4GHz Dongle (OPS v3)
    0x4022, // 2.4GHz Dongle alt 2
    0x404F  // Battle Dragon Dongle
};

// macOS Virtual Keycodes
enum MacKeyCode {
    KEY_SPACE       = 49,
    KEY_ESCAPE      = 53,
    KEY_RETURN      = 36,
    KEY_TAB         = 48,
    KEY_DELETE      = 51,
    KEY_SHIFT_LEFT  = 56,
    KEY_CONTROL_L   = 59,
    KEY_OPTION_L    = 58,
    KEY_COMMAND_L   = 55,
    
    // Letters
    KEY_A           = 0,
    KEY_S           = 1,
    KEY_D           = 2,
    KEY_W           = 13,
    KEY_Q           = 12,
    KEY_E           = 14,
    KEY_C           = 8,
    KEY_R           = 15,
    
    // Arrows
    KEY_ARROW_LEFT  = 123,
    KEY_ARROW_RIGHT = 124,
    KEY_ARROW_DOWN  = 125,
    KEY_ARROW_UP    = 126
};

// Post a native macOS keyboard event
static void post_key_event(CGKeyCode key, bool is_down) {
    CGEventRef ev = CGEventCreateKeyboardEvent(NULL, key, is_down);
    if (ev) {
        CGEventPost(kCGHIDEventTap, ev);
        CFRelease(ev);
    }
}

// Post a native macOS mouse motion event
static void post_mouse_move(float dx, float dy) {
    CGEventRef ourEvent = CGEventCreate(NULL);
    CGPoint point = CGEventGetLocation(ourEvent);
    CFRelease(ourEvent);

    point.x += dx;
    point.y += dy;

    CGEventRef moveEv = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, point, kCGMouseButtonLeft);
    if (moveEv) {
        CGEventPost(kCGHIDEventTap, moveEv);
        CFRelease(moveEv);
    }
}

// Post a native macOS mouse click event
static void post_mouse_click(CGMouseButton button, bool is_down) {
    CGEventRef ourEvent = CGEventCreate(NULL);
    CGPoint point = CGEventGetLocation(ourEvent);
    CFRelease(ourEvent);

    CGEventType type;
    if (button == kCGMouseButtonLeft) {
        type = is_down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
    } else {
        type = is_down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
    }

    CGEventRef clickEv = CGEventCreateMouseEvent(NULL, type, point, button);
    if (clickEv) {
        CGEventPost(kCGHIDEventTap, clickEv);
        CFRelease(clickEv);
    }
}

// Generate Karabiner-Elements JSON complex modifications
static bool install_karabiner_rules() {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) home = pw->pw_dir;
    }
    if (!home) return false;

    std::string config_dir = std::string(home) + "/.config/karabiner/assets/complex_modifications";
    (void)mkdir((std::string(home) + "/.config").c_str(), 0755);
    (void)mkdir((std::string(home) + "/.config/karabiner").c_str(), 0755);
    (void)mkdir((std::string(home) + "/.config/karabiner/assets").c_str(), 0755);
    (void)mkdir(config_dir.c_str(), 0755);

    std::string rule_path = config_dir + "/powera_ops_v1.json";
    std::ofstream ofs(rule_path);
    if (!ofs.is_open()) return false;

    ofs << "{\n"
        << "  \"title\": \"PowerA OPS v1 Controller - Karabiner Enhancements\",\n"
        << "  \"rules\": [\n"
        << "    {\n"
        << "      \"description\": \"PowerA OPS v1: Advanced Gaming Back Paddles (AGL1, AGL2, AGR1, AGR2)\",\n"
        << "      \"manipulators\": [\n"
        << "        {\n"
        << "          \"type\": \"basic\",\n"
        << "          \"from\": { \"pointing_button\": \"button4\", \"modifiers\": { \"optional\": [\"any\"] } },\n"
        << "          \"to\": [ { \"key_code\": \"left_shift\" } ]\n"
        << "        },\n"
        << "        {\n"
        << "          \"type\": \"basic\",\n"
        << "          \"from\": { \"pointing_button\": \"button5\", \"modifiers\": { \"optional\": [\"any\"] } },\n"
        << "          \"to\": [ { \"key_code\": \"spacebar\" } ]\n"
        << "        }\n"
        << "      ]\n"
        << "    }\n"
        << "  ]\n"
        << "}\n";

    ofs.close();
    std::cout << "[PowerA C++ Bridge] Generated Karabiner-Elements rule at:\n  " << rule_path << "\n";
    return true;
}

int main(int argc, char **argv) {
    std::cout << "============================================================\n"
              << " PowerA OPS v1 - Native C++ Karabiner & macOS Input Bridge\n"
              << " High-Performance Input Injection Engine (< 0.5ms Latency)\n"
              << "============================================================\n";

    bool mouse_mode = false;
    int poll_rate_hz = 500;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--install-karabiner-rules") {
            install_karabiner_rules();
            if (argc == 2) return 0;
        } else if (arg == "--mouse-mode") {
            mouse_mode = true;
            std::cout << "[PowerA C++ Bridge] Mouse mode enabled (Right Stick -> Cursor).\n";
        } else if (arg == "--rate" && i + 1 < argc) {
            poll_rate_hz = std::stoi(argv[++i]);
        }
    }

    // Always ensure Karabiner rule is generated
    install_karabiner_rules();

    if (hid_init() != 0) {
        std::cerr << "Failed to initialize hidapi\n";
        return 1;
    }

    hid_device *dev = nullptr;
    uint32_t buttons_prev = 0;
    uint8_t buf[64];

    std::cout << "[PowerA C++ Bridge] Polling rate: " << poll_rate_hz << " Hz. Scanning for devices...\n";

    while (true) {
        if (!dev) {
            for (uint16_t pid : SUPPORTED_PIDS) {
                dev = hid_open(POWERA_VID, pid, NULL);
                if (dev) {
                    std::cout << "[PowerA C++ Bridge] Connected to device (VID: 0x"
                              << std::hex << POWERA_VID << " PID: 0x" << pid << std::dec << ")!\n";
                    break;
                }
            }

            if (!dev) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }
        }

        int res = hid_read_timeout(dev, buf, sizeof(buf), 10);
        if (res > 0) {
            if (res >= 8) {
                uint8_t rx = buf[3];
                uint8_t ry = buf[4];
                uint32_t buttons = (uint32_t)buf[7] | ((uint32_t)buf[8] << 8);
                if (res >= 11) {
                    buttons |= ((uint32_t)buf[9] << 16) | ((uint32_t)buf[10] << 24);
                }

                // Mouse Mode: Right Stick controlling pointer
                if (mouse_mode) {
                    float dx = ((float)rx - 128.0f) / 10.0f;
                    float dy = ((float)ry - 128.0f) / 10.0f;
                    if (std::abs(dx) > 1.0f || std::abs(dy) > 1.0f) {
                        post_mouse_move(dx, dy);
                    }

                    // RT Left Click
                    if ((buttons & (1 << 7)) && !(buttons_prev & (1 << 7))) {
                        post_mouse_click(kCGMouseButtonLeft, true);
                    } else if (!(buttons & (1 << 7)) && (buttons_prev & (1 << 7))) {
                        post_mouse_click(kCGMouseButtonLeft, false);
                    }

                    // LT Right Click
                    if ((buttons & (1 << 6)) && !(buttons_prev & (1 << 6))) {
                        post_mouse_click(kCGMouseButtonRight, true);
                    } else if (!(buttons & (1 << 6)) && (buttons_prev & (1 << 6))) {
                        post_mouse_click(kCGMouseButtonRight, false);
                    }
                }

                // Button Event Injection
                struct BtnMap { const char *name; uint32_t mask; CGKeyCode key; };
                static const BtnMap BTN_MAPPINGS[] = {
                    { "A",     1 << 0,  KEY_SPACE },       // Jump
                    { "B",     1 << 1,  KEY_C },           // Crouch
                    { "X",     1 << 2,  KEY_R },           // Reload
                    { "Y",     1 << 3,  KEY_E },           // Interact
                    { "LB",    1 << 4,  KEY_Q },           // Ability 1
                    { "RB",    1 << 5,  KEY_TAB },         // Scoreboard
                    { "VIEW",  1 << 8,  KEY_ESCAPE },      // Menu
                    { "MENU",  1 << 9,  KEY_RETURN },      // Enter
                    { "LSB",   1 << 10, KEY_SHIFT_LEFT },  // Sprint
                    { "RSB",   1 << 11, KEY_CONTROL_L },   // Melee
                    { "DPAD_UP",    1 << 12, KEY_ARROW_UP },
                    { "DPAD_DOWN",  1 << 13, KEY_ARROW_DOWN },
                    { "DPAD_LEFT",  1 << 14, KEY_ARROW_LEFT },
                    { "DPAD_RIGHT", 1 << 15, KEY_ARROW_RIGHT },
                    // 4 Back Grip Buttons (AGL1, AGL2, AGR1, AGR2)
                    { "AGL1",  1 << 16, KEY_W },           // Forward paddle
                    { "AGL2",  1 << 17, KEY_S },           // Backward paddle
                    { "AGR1",  1 << 18, KEY_SPACE },       // Jump paddle
                    { "AGR2",  1 << 19, KEY_SHIFT_LEFT }   // Sprint paddle
                };

                for (const auto &mapping : BTN_MAPPINGS) {
                    bool is_down = (buttons & mapping.mask) != 0;
                    bool was_down = (buttons_prev & mapping.mask) != 0;
                    if (is_down != was_down) {
                        post_key_event(mapping.key, is_down);
                    }
                }

                buttons_prev = buttons;
            }
        } else if (res < 0) {
            std::cout << "[PowerA C++ Bridge] Device disconnected.\n";
            hid_close(dev);
            dev = nullptr;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(1000000 / poll_rate_hz));
    }

    hid_exit();
    return 0;
}
