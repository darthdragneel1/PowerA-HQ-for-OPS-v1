/**
 * PowerA PC HQ for macOS - Hardware Protocol & Packet Implementation
 * Disassembled & reverse-engineered from PowerA PC HQ v1.4.2-1
 */

#include "powera_protocol.h"
#include <string.h>
#include <stdio.h>

/**
 * Exact implementation of PowerA's 16-bit CRC algorithm from 0x45DB00.
 *
 * Assembly logic:
 *   xor ecx, ecx
 * loop:
 *   movzx eax, byte ptr [esi]
 *   movzx edx, cx
 *   add esi, 1
 *   xor eax, edx
 *   sar edx, 8
 *   mov ebx, eax
 *   movzx ecx, al
 *   and ebx, 0xf
 *   mov eax, ebx
 *   shl eax, 4
 *   xor eax, ecx
 *   mov ecx, eax
 *   add eax, eax
 *   shr ecx, 4
 *   xor eax, ecx
 *   xor ecx, edx
 *   shl eax, 4
 *   xor ebx, eax
 *   shl ebx, 3
 *   xor ecx, ebx
 *   cmp edi, esi
 *   jne loop
 */
void powera_calculate_crc(const uint8_t *buf, size_t start_offset, size_t end_offset, uint8_t *out_crc_hi, uint8_t *out_crc_lo) {
    uint16_t cx = 0;
    for (size_t i = start_offset; i < end_offset; i++) {
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
    
    if (out_crc_hi) *out_crc_hi = (uint8_t)((cx >> 8) & 0xFF);
    if (out_crc_lo) *out_crc_lo = (uint8_t)(cx & 0xFF);
}

void powera_build_packet(uint8_t *packet, uint8_t opcode, const uint8_t *payload, size_t payload_len) {
    if (!packet) return;
    memset(packet, 0, POWERA_PACKET_TOTAL_SIZE);
    
    // Header
    packet[0] = POWERA_PACKET_MAGIC; // 0xE1
    packet[1] = 0x00;
    packet[2] = 0x01;
    packet[3] = POWERA_PAYLOAD_SIZE; // 0x3C
    packet[4] = opcode;
    
    if (payload && payload_len > 0) {
        size_t max_copy = payload_len;
        if (max_copy > POWERA_PAYLOAD_SIZE - 3) {
            max_copy = POWERA_PAYLOAD_SIZE - 3;
        }
        memcpy(&packet[5], payload, max_copy);
    }
    
    // Compute CRC on bytes 4 through 61
    uint8_t crc_hi = 0, crc_lo = 0;
    powera_calculate_crc(packet, 4, 62, &crc_hi, &crc_lo);
    packet[62] = crc_hi;
    packet[63] = crc_lo;
}

void powera_build_rumble_packet(uint8_t *packet, uint8_t left_grip, uint8_t right_grip, uint8_t left_trigger, uint8_t right_trigger) {
    if (!packet) return;
    memset(packet, 0, POWERA_PACKET_TOTAL_SIZE);
    static uint8_t s_rumble_seq = 0;
    
    // As disassembled at 0x40C5F4 - 0x40C63B:
    packet[0] = POWERA_CMD_RUMBLE; // 0x20
    packet[1] = 0x09;
    packet[2] = s_rumble_seq++;
    packet[3] = 0x64; // Frequency / duty
    packet[4] = 0x09;
    packet[5] = 0x00;
    packet[6] = 0x0F;
    packet[7] = 0x32;
    packet[8] = 0x32;
    packet[9] = left_grip;      // Left Main Motor
    packet[10] = right_grip;    // Right Main Motor
    packet[11] = left_trigger;  // Left Trigger Impulse
    packet[12] = right_trigger; // Right Trigger Impulse
    packet[13] = 0x00;
    if (left_grip == 0 && right_grip == 0 && left_trigger == 0 && right_trigger == 0) {
        packet[14] = 0x00; // Stop / Disable all motors
    } else {
        packet[14] = 0xFF; // Enable motors
    }
}

bool powera_pwa_parse(const uint8_t *pwa_data, size_t data_len, powera_config_t *config) {
    if (!pwa_data || data_len < POWERA_PROFILE_DATA_SIZE || !config) {
        return false;
    }
    
    memset(config, 0, sizeof(powera_config_t));
    snprintf(config->profile_name, sizeof(config->profile_name), "Profile");
    
    // Parse button mappings (Offsets 0 - 19)
    config->map_btn_a    = pwa_data[0];
    config->map_btn_b    = pwa_data[1];
    config->map_btn_x    = pwa_data[2];
    config->map_btn_y    = pwa_data[3];
    config->map_btn_lb   = pwa_data[4];
    config->map_btn_rb   = pwa_data[5];
    config->map_btn_agl1 = pwa_data[6];
    config->map_btn_agr1 = pwa_data[7];
    config->map_btn_agl2 = pwa_data[8];
    config->map_btn_agr2 = pwa_data[9];
    config->map_btn_view = pwa_data[10];
    config->map_btn_menu = pwa_data[11];
    
    // Stick Deadzones & Curves (Offsets 20 - 49)
    config->stick_l_inner_deadzone = pwa_data[20];
    config->stick_l_outer_deadzone = pwa_data[23];
    config->stick_l_curve          = (powera_stick_curve_t)(pwa_data[25] % 4);
    
    config->stick_r_inner_deadzone = pwa_data[30];
    config->stick_r_outer_deadzone = pwa_data[33];
    config->stick_r_curve          = (powera_stick_curve_t)(pwa_data[35] % 4);
    
    // Trigger Stops & Deadzones (Offsets 50 - 65)
    // Remember diff: T1=99 (0x63), T2=50 (0x32), T3=0 (0x00)
    config->trigger_l_end_deadzone   = pwa_data[50];
    config->trigger_r_end_deadzone   = pwa_data[52];
    config->trigger_l_start_deadzone = pwa_data[54];
    config->trigger_r_start_deadzone = pwa_data[56];
    
    // Vibration Motor Strengths (Offsets 66 - 75)
    config->motor_left_grip     = pwa_data[66];
    config->motor_right_grip    = pwa_data[67];
    config->motor_left_trigger  = pwa_data[68];
    config->motor_right_trigger = pwa_data[69];
    
    // RGB Settings (Offsets 120 - 160)
    config->rgb_mode        = (powera_rgb_mode_t)(pwa_data[126] % 5);
    config->rgb_zone        = (powera_rgb_zone_t)(pwa_data[127] % 5);
    config->rgb_direction   = (powera_rgb_direction_t)(pwa_data[128] % 4);
    config->rgb_brightness  = pwa_data[130];
    config->rgb_speed       = pwa_data[131];
    config->rgb_r           = pwa_data[132];
    config->rgb_g           = pwa_data[133];
    config->rgb_b           = pwa_data[134];
    
    return true;
}

bool powera_pwa_serialize(const powera_config_t *config, uint8_t *out_pwa, size_t out_max_len) {
    if (!config || !out_pwa || out_max_len < POWERA_PROFILE_DATA_SIZE) {
        return false;
    }
    
    // Start with blank template
    memset(out_pwa, 0, POWERA_PROFILE_DATA_SIZE);
    
    // Set default headers
    out_pwa[0] = config->map_btn_a ? config->map_btn_a : 0x12;
    out_pwa[1] = config->map_btn_b ? config->map_btn_b : 0x13;
    out_pwa[2] = config->map_btn_x ? config->map_btn_x : 0x14;
    out_pwa[3] = config->map_btn_y ? config->map_btn_y : 0x15;
    out_pwa[4] = config->map_btn_lb ? config->map_btn_lb : 0x16;
    out_pwa[5] = config->map_btn_rb ? config->map_btn_rb : 0x17;
    out_pwa[6] = config->map_btn_agl1 ? config->map_btn_agl1 : 0x20;
    out_pwa[7] = config->map_btn_agr1 ? config->map_btn_agr1 : 0x21;
    out_pwa[8] = config->map_btn_agl2 ? config->map_btn_agl2 : 0x22;
    out_pwa[9] = config->map_btn_agr2 ? config->map_btn_agr2 : 0x23;
    out_pwa[10] = config->map_btn_view ? config->map_btn_view : 0x24;
    out_pwa[11] = config->map_btn_menu ? config->map_btn_menu : 0x25;
    
    // Default calibration pads
    for (int i = 14; i <= 19; i++) out_pwa[i] = 0x40;
    
    // Stick deadzones
    out_pwa[20] = config->stick_l_inner_deadzone;
    out_pwa[23] = config->stick_l_outer_deadzone ? config->stick_l_outer_deadzone : 0x14;
    out_pwa[25] = (uint8_t)config->stick_l_curve;
    
    out_pwa[30] = config->stick_r_inner_deadzone;
    out_pwa[33] = config->stick_r_outer_deadzone ? config->stick_r_outer_deadzone : 0x14;
    out_pwa[35] = (uint8_t)config->stick_r_curve;
    
    // Trigger stops
    out_pwa[50] = config->trigger_l_end_deadzone;
    out_pwa[52] = config->trigger_r_end_deadzone;
    out_pwa[54] = config->trigger_l_start_deadzone;
    out_pwa[56] = config->trigger_r_start_deadzone;
    out_pwa[58] = config->trigger_l_end_deadzone;
    out_pwa[60] = config->trigger_r_end_deadzone;
    
    // Vibration motors
    out_pwa[66] = config->motor_left_grip;
    out_pwa[67] = config->motor_right_grip;
    out_pwa[68] = config->motor_left_trigger;
    out_pwa[69] = config->motor_right_trigger;
    
    // RGB parameters
    out_pwa[126] = (uint8_t)config->rgb_mode;
    out_pwa[127] = (uint8_t)config->rgb_zone;
    out_pwa[128] = (uint8_t)config->rgb_direction;
    out_pwa[130] = config->rgb_brightness;
    out_pwa[131] = config->rgb_speed;
    out_pwa[132] = config->rgb_r;
    out_pwa[133] = config->rgb_g;
    out_pwa[134] = config->rgb_b;
    
    return true;
}
