/**
 * PowerA PC HQ for macOS - Hardware Protocol & Packet Definitions
 * Reverse-Engineered from PowerA PC HQ v1.4.2-1 (PowerA OPS v1 / Battle Dragon series)
 * 
 * Target Devices:
 * - PowerA OPS v1 Wireless Controller (PID: 0x4025, 0x401B)
 * - PowerA OPS v3 Pro Wireless Controller (PID: 0x4032)
 * - PowerA 2.4GHz Wireless Dongle (PID: 0x400C, 0x4013, 0x401E, 0x4022, 0x404F)
 * - PowerA Battle Dragon Series (PID: 0x400E, 0x402E, 0x4039, 0x4017)
 * - Vendor ID: 0x20D6 (PowerA / Dashine Electronics)
 */

#ifndef POWERA_PROTOCOL_H
#define POWERA_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// USB Identifiers
#define POWERA_VENDOR_ID                     0x20D6

// Known Product IDs
#define POWERA_PID_OPS_V1                   0x4025  // PowerA OPS v1 Wireless Controller
#define POWERA_PID_OPS_V1_FORTNITE          0x401B  // PowerA OPS v1 Fortnite Edition
#define POWERA_PID_OPS_V3_PRO               0x4032  // PowerA OPS v3 Pro
#define POWERA_PID_BATTLE_DRAGON_PRO        0x402E  // Battle Dragon Pro
#define POWERA_PID_BATTLE_DRAGON            0x404F  // Battle Dragon standard
#define POWERA_PID_BATTLE_DRAGON_ADV        0x4017  // Battle Dragon Adv
#define POWERA_PID_BATTLE_DRAGON_ADV_L      0x4039  // Battle Dragon Adv L

// 2.4GHz Wireless Dongle Product IDs
#define POWERA_PID_DONGLE_OPS_V1            0x4027  // PowerA OPS v1 Wireless Controller - Adapter (Dongle)
#define POWERA_PID_DONGLE_1531A             0x400C  // 2.4GHz Dongle (OPS v1 alternate)
#define POWERA_PID_DONGLE_1531B             0x4013  // 2.4GHz Dongle alt
#define POWERA_PID_DONGLE_1532A             0x401E  // 2.4GHz Dongle (OPS v3)
#define POWERA_PID_DONGLE_1532B             0x4022  // 2.4GHz Dongle alt

// Protocol Constants
#define POWERA_PACKET_MAGIC                 0xE1
#define POWERA_PACKET_TOTAL_SIZE            64      // 0x40 bytes
#define POWERA_PAYLOAD_SIZE                 60      // 0x3C bytes
#define POWERA_PROFILE_DATA_SIZE            173     // Authentic .pwa binary profile length

// Command Opcodes (Reverse-engineered from 0x45DC3D - 0x45F37E)
#define POWERA_CMD_RUMBLE                   0x20    // 4-Motor Rumble Control
#define POWERA_CMD_READ_PROFILE             0xAD    // Read Profile Chunks (sub 0..6)
#define POWERA_CMD_WRITE_PROFILE            0xAE    // Write Profile Chunks (sub 0..6)
#define POWERA_CMD_STICK_DEADZONE           0xA2    // Stick deadzones & curve config
#define POWERA_CMD_TRIGGER_DEADZONE         0xB5    // Trigger start/stop calibration
#define POWERA_CMD_RGB_CONFIG               0xA6    // RGB zones & modes
#define POWERA_CMD_RGB_CUSTOM_COLOR         0xA8    // RGB static hex colors
#define POWERA_CMD_RGB_DYNAMIC              0xAA    // RGB speed & brightness
#define POWERA_CMD_CALIBRATION_START        0xB1    // Recalibration start
#define POWERA_CMD_CALIBRATION_STEP         0xB2    // Recalibration step
#define POWERA_CMD_CALIBRATION_FINISH       0xB3    // Recalibration complete
#define POWERA_CMD_DONGLE_PAIR_SYNC         0xC2    // Dongle wireless pairing / sync status
#define POWERA_CMD_FW_VERSION               0xF1    // Query firmware version
#define POWERA_CMD_BATTERY_STATUS           0xFB    // Query battery level & charging state

// Connection Modes
typedef enum {
    POWERA_CONN_DISCONNECTED = 0,
    POWERA_CONN_WIRED_USB    = 1,
    POWERA_CONN_DONGLE_2_4G  = 2,
    POWERA_CONN_BLUETOOTH    = 3
} powera_connection_mode_t;

// Trigger Lock Stops
typedef enum {
    POWERA_TRIGGER_LOCK_T1 = 0, // Full pull (100% travel - Normal gaming)
    POWERA_TRIGGER_LOCK_T2 = 1, // Medium pull (50% travel - Fast firing)
    POWERA_TRIGGER_LOCK_T3 = 2  // Hair trigger (Instant digital - FPS hair-trigger)
} powera_trigger_stop_t;

// Stick Response Curves
typedef enum {
    POWERA_CURVE_LINEAR     = 0,
    POWERA_CURVE_SMOOTH     = 1,
    POWERA_CURVE_AGGRESSIVE = 2,
    POWERA_CURVE_INSTANT    = 3
} powera_stick_curve_t;

// RGB Modes
typedef enum {
    POWERA_RGB_MODE_STATIC       = 0, // Solid Color
    POWERA_RGB_MODE_BREATHING    = 1, // Breathing Glow
    POWERA_RGB_MODE_COLOR_CYCLE  = 2, // Color Cycle
    POWERA_RGB_MODE_RAINBOW      = 3, // Rainbow Wave
    POWERA_RGB_MODE_FLAME_RIPPLE = 4  // Flame Ripple (Battle Dragon)
} powera_rgb_mode_t;

// RGB Zones
typedef enum {
    POWERA_ZONE_ALL         = 0,
    POWERA_ZONE_DRAGON_HEAD = 1,
    POWERA_ZONE_FLAME       = 2,
    POWERA_ZONE_DRAGON_BODY = 3,
    POWERA_ZONE_CUSTOM      = 4
} powera_rgb_zone_t;

// RGB Rainbow Direction
typedef enum {
    POWERA_DIR_LEFT_TO_RIGHT = 0,
    POWERA_DIR_RIGHT_TO_LEFT = 1,
    POWERA_DIR_TOP_TO_BOTTOM = 2,
    POWERA_DIR_BOTTOM_TO_TOP = 3
} powera_rgb_direction_t;

// Standard Button Bitmasks
#define POWERA_BTN_A            (1 << 0)
#define POWERA_BTN_B            (1 << 1)
#define POWERA_BTN_X            (1 << 2)
#define POWERA_BTN_Y            (1 << 3)
#define POWERA_BTN_LB           (1 << 4)
#define POWERA_BTN_RB           (1 << 5)
#define POWERA_BTN_LT_DIGITAL   (1 << 6)
#define POWERA_BTN_RT_DIGITAL   (1 << 7)
#define POWERA_BTN_VIEW         (1 << 8)  // Back / View
#define POWERA_BTN_MENU         (1 << 9)  // Start / Menu
#define POWERA_BTN_LSB          (1 << 10) // L3 / Left Thumb Click
#define POWERA_BTN_RSB          (1 << 11) // R3 / Right Thumb Click
#define POWERA_BTN_DPAD_UP      (1 << 12)
#define POWERA_BTN_DPAD_DOWN    (1 << 13)
#define POWERA_BTN_DPAD_LEFT    (1 << 14)
#define POWERA_BTN_DPAD_RIGHT   (1 << 15)

// Advanced Gaming Buttons (Back paddles on OPS v1 / OPS v3 Pro)
#define POWERA_BTN_AGL1         (1 << 16) // Left Upper Grip
#define POWERA_BTN_AGL2         (1 << 17) // Left Lower Grip
#define POWERA_BTN_AGR1         (1 << 18) // Right Upper Grip
#define POWERA_BTN_AGR2         (1 << 19) // Right Lower Grip
#define POWERA_BTN_GUIDE        (1 << 20) // Home / Xbox / Guide

// Live Controller State
typedef struct {
    bool connected;
    powera_connection_mode_t connection_mode;
    char model_name[64];
    uint16_t vendor_id;
    uint16_t product_id;
    uint8_t battery_percent;
    bool is_charging;
    uint8_t firmware_major;
    uint8_t firmware_minor;
    uint8_t firmware_patch;
    uint8_t current_profile_id;
    
    // Analog Inputs (Normalized -1.0 to +1.0 for sticks, 0.0 to 1.0 for triggers)
    float stick_lx;
    float stick_ly;
    float stick_rx;
    float stick_ry;
    float trigger_lt;
    float trigger_rt;
    
    // Buttons (Bitmask)
    uint32_t buttons;
    
    // Telemetry & Diagnostic Metrics
    uint64_t report_count;
    uint32_t report_rate_hz;
    char last_report_hex[64];
    
    // Physical Trigger Locks
    powera_trigger_stop_t trigger_lock_l;
    powera_trigger_stop_t trigger_lock_r;
} powera_controller_state_t;

// Complete Controller Configuration Struct (Maps directly to 173-byte .pwa format)
typedef struct {
    char profile_name[32];
    uint8_t profile_id; // 1 = T1, 2 = T2, 3 = T3, 4+ = Custom
    
    // Button Remapping (Maps each button to desired virtual action/button)
    uint8_t map_btn_a;
    uint8_t map_btn_b;
    uint8_t map_btn_x;
    uint8_t map_btn_y;
    uint8_t map_btn_lb;
    uint8_t map_btn_rb;
    uint8_t map_btn_view;
    uint8_t map_btn_menu;
    uint8_t map_btn_agl1;
    uint8_t map_btn_agl2;
    uint8_t map_btn_agr1;
    uint8_t map_btn_agr2;
    
    // Left Stick Settings
    uint8_t stick_l_inner_deadzone; // 0 - 100%
    uint8_t stick_l_outer_deadzone; // 0 - 100%
    powera_stick_curve_t stick_l_curve;
    
    // Right Stick Settings
    uint8_t stick_r_inner_deadzone;
    uint8_t stick_r_outer_deadzone;
    powera_stick_curve_t stick_r_curve;
    
    // Triggers Deadzones & Lock Calibration
    uint8_t trigger_l_start_deadzone; // 0 - 100%
    uint8_t trigger_l_end_deadzone;   // 0 - 100% (99 for T1, 50 for T2, 0 for T3)
    uint8_t trigger_r_start_deadzone;
    uint8_t trigger_r_end_deadzone;
    bool trigger_sync_enabled;
    
    // 4-Motor Vibration Strengths (0 - 100%)
    uint8_t motor_left_grip;     // Left Main Rotor
    uint8_t motor_right_grip;    // Right Main Rotor
    uint8_t motor_left_trigger;  // Left Trigger Impulse Motor
    uint8_t motor_right_trigger; // Right Trigger Impulse Motor
    
    // RGB Lighting Studio
    powera_rgb_mode_t rgb_mode;
    powera_rgb_zone_t rgb_zone;
    powera_rgb_direction_t rgb_direction;
    uint8_t rgb_brightness; // 0 - 100%
    uint8_t rgb_speed;      // 0 - 100%
    uint8_t rgb_r;
    uint8_t rgb_g;
    uint8_t rgb_b;
} powera_config_t;

/**
 * Calculate the exact proprietary 16-bit CRC for a PowerA packet.
 * Disassembled directly from function at 0x45DB00 in PowerA_PC_HQ.exe.
 * 
 * @param buf Pointer to the packet buffer.
 * @param start_offset Starting index to hash (usually 4).
 * @param end_offset Ending index (usually 0x3E / 62).
 * @param out_crc_hi Pointer to receive high byte of CRC.
 * @param out_crc_lo Pointer to receive low byte of CRC.
 */
void powera_calculate_crc(const uint8_t *buf, size_t start_offset, size_t end_offset, uint8_t *out_crc_hi, uint8_t *out_crc_lo);

/**
 * Builds a standardized 64-byte command packet with 0xE1 header and valid CRC.
 */
void powera_build_packet(uint8_t *packet, uint8_t opcode, const uint8_t *payload, size_t payload_len);

/**
 * Builds the 4-motor rumble packet (Command 0x20).
 * Left strong, right weak, left trigger impulse, right trigger impulse (0 - 255).
 */
void powera_build_rumble_packet(uint8_t *packet, uint8_t left_grip, uint8_t right_grip, uint8_t left_trigger, uint8_t right_trigger);

/**
 * Parse an authentic 173-byte .pwa profile file into powera_config_t.
 */
bool powera_pwa_parse(const uint8_t *pwa_data, size_t data_len, powera_config_t *config);

/**
 * Serialize a powera_config_t into the authentic 173-byte .pwa format.
 */
bool powera_pwa_serialize(const powera_config_t *config, uint8_t *out_pwa, size_t out_max_len);

#ifdef __cplusplus
}
#endif

#endif // POWERA_PROTOCOL_H
