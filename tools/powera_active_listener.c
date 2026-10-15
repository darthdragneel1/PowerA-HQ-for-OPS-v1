/**
 * PowerA PC HQ for macOS - Multi-Stage Active Hardware Listener & Manual Input Mapper
 * 
 * Stages:
 *   Stage 1: Direct IOKit USB Interface 0 / Pipe Interrupt Reader (Bypasses macOS HID)
 *   Stage 2: hidapi USB Interface 1 & Bluetooth GATT HID Reader
 *   Stage 3: Real-Time Bitwise Packet Differ & Visualizer
 *   Stage 4: Interactive Manual Mapping Wizard -> Generates profiles/hardware_map.json
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>
#include <signal.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#if __has_include(<hidapi/hidapi.h>)
#include <hidapi/hidapi.h>
#elif __has_include(<hidapi.h>)
#include <hidapi.h>
#elif __has_include("/opt/homebrew/include/hidapi/hidapi.h")
#include "/opt/homebrew/include/hidapi/hidapi.h"
#else
#include <hidapi/hidapi.h>
#endif

#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_MAGENTA "\033[35m"

typedef struct {
    char name[32];
    uint8_t byte_offset;
    uint8_t bit_mask;
    bool is_axis;
    int min_val;
    int max_val;
} mapped_input_t;

typedef struct {
    char model_name[64];
    uint16_t vid;
    uint16_t pid;
    mapped_input_t inputs[64];
    int input_count;
} hardware_map_t;

static volatile sig_atomic_t g_running = 1;
static pthread_mutex_t g_print_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_paused_driver = false;

static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

// Stage 1 Context (Direct IOKit USB Interface 0 Pipe)
typedef struct {
    IOUSBInterfaceInterface **intf;
    UInt8 in_pipe;
    UInt8 out_pipe;
    uint16_t vid;
    uint16_t pid;
    char dev_name[64];
    bool active;
    uint8_t last_pkt[64];
    UInt32 last_sz;
    bool has_baseline;
} iokit_stage_t;

// Stage 2 Context (HIDAPI Interface 1 / Bluetooth)
typedef struct {
    hid_device *dev;
    char path[256];
    bool active;
    uint8_t last_pkt[64];
    int last_sz;
    bool has_baseline;
} hid_stage_t;

static iokit_stage_t g_stage1;
static hid_stage_t g_stage2;

// Print packet diff with ANSI highlights
static void print_packet_diff(const char *source, const uint8_t *cur, UInt32 sz, const uint8_t *prev, bool has_prev) {
    pthread_mutex_lock(&g_print_lock);
    printf("%s[%s]%s (Len=%u): ", COLOR_CYAN, source, COLOR_RESET, sz);
    for (UInt32 i = 0; i < sz && i < 24; i++) {
        if (has_prev && cur[i] != prev[i]) {
            printf("%s%s%02X%s ", COLOR_BOLD, COLOR_YELLOW, cur[i], COLOR_RESET);
        } else {
            printf("%02X ", cur[i]);
        }
    }
    if (sz > 24) printf("...");

    if (has_prev) {
        bool first = true;
        for (UInt32 i = 0; i < sz && i < 24; i++) {
            if (cur[i] != prev[i]) {
                uint8_t diff = cur[i] ^ prev[i];
                if (first) {
                    printf(" | %sChanges:%s", COLOR_GREEN, COLOR_RESET);
                    first = false;
                }
                printf(" [B%u: 0x%02X->0x%02X (diff=0x%02X)]", i, prev[i], cur[i], diff);
            }
        }
    }
    printf("\n");
    pthread_mutex_unlock(&g_print_lock);
}

// Stage 1 Thread: Native IOKit Pipe Listener (500Hz)
static void* stage1_thread(void *arg) {
    iokit_stage_t *s = (iokit_stage_t*)arg;
    uint8_t buf[64];
    while (g_running && s->active) {
        UInt32 sz = sizeof(buf);
        IOReturn ret = (*s->intf)->ReadPipe(s->intf, s->in_pipe, buf, &sz);
        if (ret == kIOReturnSuccess && sz > 0) {
            bool changed = (!s->has_baseline) || (sz != s->last_sz) || (memcmp(buf, s->last_pkt, sz) != 0);
            if (changed) {
                print_packet_diff("Stage 1 (IOKit USB Pipe)", buf, sz, s->last_pkt, s->has_baseline);
                memcpy(s->last_pkt, buf, sz);
                s->last_sz = sz;
                s->has_baseline = true;
            }
        } else if (ret == kIOReturnNoDevice || ret == kIOReturnNotOpen) {
            s->active = false;
            break;
        }
        usleep(2000); // 2ms polling (~500Hz)
    }
    return NULL;
}

// Stage 2 Thread: HIDAPI Listener
static void* stage2_thread(void *arg) {
    hid_stage_t *s = (hid_stage_t*)arg;
    uint8_t buf[64];
    while (g_running && s->active) {
        int sz = hid_read_timeout(s->dev, buf, sizeof(buf), 20);
        if (sz > 0) {
            bool changed = (!s->has_baseline) || (sz != s->last_sz) || (memcmp(buf, s->last_pkt, (size_t)sz) != 0);
            if (changed) {
                print_packet_diff("Stage 2 (HID / Config)", buf, (UInt32)sz, s->last_pkt, s->has_baseline);
                memcpy(s->last_pkt, buf, (size_t)sz);
                s->last_sz = sz;
                s->has_baseline = true;
            }
        } else if (sz < 0) {
            s->active = false;
            break;
        }
        usleep(2000);
    }
    return NULL;
}

// Discover and open Stage 1 (Direct IOUSBHostInterface match)
static bool setup_stage1(void) {
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostInterface");
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS) return false;

    io_service_t intf_service;
    while ((intf_service = IOIteratorNext(iter)) && !g_stage1.active) {
        io_service_t parent = 0;
        IORegistryEntryGetParentEntry(intf_service, kIOServicePlane, &parent);
        if (parent) {
            CFNumberRef vidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(parent, CFSTR("idVendor"), kCFAllocatorDefault, 0);
            CFNumberRef pidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(parent, CFSTR("idProduct"), kCFAllocatorDefault, 0);
            CFStringRef prodRef = (CFStringRef)IORegistryEntryCreateCFProperty(parent, CFSTR("USB Product Name"), kCFAllocatorDefault, 0);

            int vid = 0, pid = 0;
            if (vidRef) { CFNumberGetValue(vidRef, kCFNumberIntType, &vid); CFRelease(vidRef); }
            if (pidRef) { CFNumberGetValue(pidRef, kCFNumberIntType, &pid); CFRelease(pidRef); }

            CFNumberRef ifNumRef = (CFNumberRef)IORegistryEntryCreateCFProperty(intf_service, CFSTR("bInterfaceNumber"), kCFAllocatorDefault, 0);
            int ifNum = -1;
            if (ifNumRef) { CFNumberGetValue(ifNumRef, kCFNumberIntType, &ifNum); CFRelease(ifNumRef); }

            if ((vid == 0x20D6 || vid == 0x24C6 || vid == 0x2E2C || vid == 0x045E) && ifNum == 0) {
                g_stage1.vid = (uint16_t)vid;
                g_stage1.pid = (uint16_t)pid;
                if (prodRef) {
                    CFStringGetCString(prodRef, g_stage1.dev_name, sizeof(g_stage1.dev_name), kCFStringEncodingUTF8);
                    CFRelease(prodRef);
                } else {
                    snprintf(g_stage1.dev_name, sizeof(g_stage1.dev_name), "PowerA USB (0x%04X:0x%04X)", vid, pid);
                }

                IOCFPlugInInterface **plugIn = NULL;
                SInt32 score = 0;
                HRESULT res = IOCreatePlugInInterfaceForService(intf_service, kIOUSBInterfaceUserClientTypeID,
                                                                kIOCFPlugInInterfaceID, &plugIn, &score);
                if (res == S_OK && plugIn) {
                    IOUSBInterfaceInterface **intf = NULL;
                    (*plugIn)->QueryInterface(plugIn, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID*)&intf);
                    (*plugIn)->Release(plugIn);
                    if (intf) {
                        IOReturn open_res = (*intf)->USBInterfaceOpen(intf);
                        if (open_res != KERN_SUCCESS) {
                            // If powera-driver holds exclusive lock, unload driver daemon temporarily
                            printf("  [Info] Interface 0 locked by background daemon. Unloading driver for active listening...\n");
                            system("launchctl unload ~/Library/LaunchAgents/com.powera.driver.plist 2>/dev/null; killall powera-driver 2>/dev/null");
                            g_paused_driver = true;
                            usleep(400000);
                            open_res = (*intf)->USBInterfaceOpen(intf);
                        }

                        if (open_res == KERN_SUCCESS) {
                            UInt8 numEndpoints = 0;
                            (*intf)->GetNumEndpoints(intf, &numEndpoints);
                            for (UInt8 p = 1; p <= numEndpoints; p++) {
                                UInt8 dir = 0, num = 0, type = 0, interval = 0;
                                UInt16 maxPkt = 0;
                                (*intf)->GetPipeProperties(intf, p, &dir, &num, &type, &maxPkt, &interval);
                                if (dir == kUSBIn && type == kUSBInterrupt && g_stage1.in_pipe == 0) g_stage1.in_pipe = p;
                                if (dir == kUSBOut && type == kUSBInterrupt && g_stage1.out_pipe == 0) g_stage1.out_pipe = p;
                            }
                            if (g_stage1.in_pipe > 0) {
                                g_stage1.intf = intf;
                                g_stage1.active = true;

                                // Send XID GET_CAPABILITIES (0xC1, 0x01, 0x0100, 0x0000, 20) to wake controller from standby
                                uint8_t cap_buf[64] = {0};
                                IOUSBDevRequest cap_req;
                                memset(&cap_req, 0, sizeof(cap_req));
                                cap_req.bmRequestType = 0xC1;
                                cap_req.bRequest = 0x01; // GET_CAPABILITIES
                                cap_req.wValue = 0x0100; // Input Capabilities
                                cap_req.wIndex = 0x0000;
                                cap_req.wLength = 20;
                                cap_req.pData = cap_buf;
                                (*intf)->ControlRequest(intf, 0, &cap_req);

                                if (g_stage1.out_pipe > 0) {
                                    uint8_t led[3] = {0x01, 0x03, 0x02}; // Player 1 Solid ON
                                    (*intf)->WritePipe(intf, g_stage1.out_pipe, led, 3);
                                }
                            } else {
                                (*intf)->USBInterfaceClose(intf);
                                (*intf)->Release(intf);
                            }
                        } else {
                            (*intf)->Release(intf);
                        }
                    }
                }
            }
            IOObjectRelease(parent);
        }
        IOObjectRelease(intf_service);
    }
    IOObjectRelease(iter);
    return g_stage1.active;
}

// Discover and open Stage 2 (HIDAPI)
static bool setup_stage2(void) {
    if (hid_init() != 0) return false;
    struct hid_device_info *devs = hid_enumerate(0x20D6, 0x0);
    if (!devs) devs = hid_enumerate(0x24C6, 0x0);
    if (!devs) devs = hid_enumerate(0x2E2C, 0x0);
    if (!devs) devs = hid_enumerate(0x045E, 0x0);

    if (devs) {
        g_stage2.dev = hid_open_path(devs->path);
        if (g_stage2.dev) {
            strncpy(g_stage2.path, devs->path, sizeof(g_stage2.path) - 1);
            g_stage2.active = true;
        }
        hid_free_enumeration(devs);
    }
    return g_stage2.active;
}

// Interactive Manual Mapping Wizard
static void run_mapping_wizard(void) {
    printf("\n%s========================================================%s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s   PowerA Hardware Input Mapping Wizard                 %s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s========================================================%s\n", COLOR_BOLD, COLOR_RESET);
    printf("Feasibility confirmed: Direct bit-level differ maps hardware without official SDKs.\n\n");

    const char *target_inputs[] = {
        "A Button", "B Button", "X Button", "Y Button",
        "D-Pad UP", "D-Pad DOWN", "D-Pad LEFT", "D-Pad RIGHT",
        "Left Bumper (LB)", "Right Bumper (RB)",
        "Left Stick Click (LSB)", "Right Stick Click (RSB)",
        "Menu Button (Start)", "View Button (Back)", "Guide / Home Button",
        "Left Trigger (LT - Pull fully)", "Right Trigger (RT - Pull fully)",
        "Rear Paddle AGL1 (Top Left)", "Rear Paddle AGL2 (Bottom Left)",
        "Rear Paddle AGR1 (Top Right)", "Rear Paddle AGR2 (Bottom Right)"
    };
    int num_inputs = sizeof(target_inputs) / sizeof(target_inputs[0]);

    hardware_map_t map;
    memset(&map, 0, sizeof(map));
    map.vid = g_stage1.active ? g_stage1.vid : 0x20D6;
    map.pid = g_stage1.active ? g_stage1.pid : 0x4027;
    strncpy(map.model_name, g_stage1.active ? g_stage1.dev_name : "PowerA OPS v1", sizeof(map.model_name) - 1);

    for (int i = 0; i < num_inputs && g_running; i++) {
        printf("\n[%d/%d] %sACTION REQUIRED:%s Press & HOLD %s%s%s (Ctrl+C to abort)\n",
               i + 1, num_inputs, COLOR_YELLOW, COLOR_RESET, COLOR_BOLD, target_inputs[i], COLOR_RESET);

        uint8_t base[64] = {0};
        uint8_t pressed[64] = {0};
        UInt32 base_sz = sizeof(base), pressed_sz = sizeof(pressed);

        // Capture idle baseline
        usleep(300000);
        if (g_stage1.active) {
            (*g_stage1.intf)->ReadPipeTO(g_stage1.intf, g_stage1.in_pipe, base, &base_sz, 60, 60);
        } else if (g_stage2.active) {
            int sz = hid_read_timeout(g_stage2.dev, base, sizeof(base), 60);
            if (sz > 0) base_sz = (UInt32)sz;
        }

        bool detected = false;
        int diff_byte = -1;
        uint8_t diff_bitmask = 0;

        for (int retry = 0; retry < 120 && !detected && g_running; retry++) {
            pressed_sz = sizeof(pressed);
            bool got_pkt = false;
            if (g_stage1.active) {
                IOReturn ret = (*g_stage1.intf)->ReadPipeTO(g_stage1.intf, g_stage1.in_pipe, pressed, &pressed_sz, 50, 50);
                if (ret == kIOReturnSuccess && pressed_sz > 0) got_pkt = true;
            } else if (g_stage2.active) {
                int sz = hid_read_timeout(g_stage2.dev, pressed, sizeof(pressed), 50);
                if (sz > 0) { pressed_sz = (UInt32)sz; got_pkt = true; }
            }

            if (got_pkt) {
                for (UInt32 b = 0; b < pressed_sz; b++) {
                    if (pressed[b] != base[b]) {
                        diff_byte = (int)b;
                        diff_bitmask = pressed[b] ^ base[b];
                        detected = true;
                        break;
                    }
                }
            }
            usleep(40000); // 40ms
        }

        if (detected) {
            printf("  %s✓ Mapped!%s Byte offset: %d | Bitmask: 0x%02X\n", COLOR_GREEN, COLOR_RESET, diff_byte, diff_bitmask);
            mapped_input_t *inp = &map.inputs[map.input_count++];
            inp->byte_offset = (uint8_t)diff_byte;
            inp->bit_mask = diff_bitmask;
            inp->is_axis = (strstr(target_inputs[i], "Trigger") != NULL);
            strncpy(inp->name, target_inputs[i], sizeof(inp->name) - 1);
            printf("  -> Release input now...\n");
            usleep(500000);
        } else {
            printf("  %s[Timeout]%s No input detected for %s. Skipping.\n", COLOR_RED, COLOR_RESET, target_inputs[i]);
        }
    }

    // Export mapping definition
    system("mkdir -p profiles");
    const char *out_path = "profiles/hardware_map.json";
    FILE *f = fopen(out_path, "w");
    if (f) {
        fprintf(f, "{\n");
        fprintf(f, "  \"model\": \"%s\",\n", map.model_name);
        fprintf(f, "  \"vid\": \"0x%04X\",\n", map.vid);
        fprintf(f, "  \"pid\": \"0x%04X\",\n", map.pid);
        fprintf(f, "  \"generated_at\": \"macOS-Live-Mapper\",\n");
        fprintf(f, "  \"mappings\": [\n");
        for (int i = 0; i < map.input_count; i++) {
            fprintf(f, "    {\"name\": \"%s\", \"byte\": %d, \"mask\": \"0x%02X\", \"is_axis\": %s}%s\n",
                    map.inputs[i].name, map.inputs[i].byte_offset, map.inputs[i].bit_mask,
                    map.inputs[i].is_axis ? "true" : "false",
                    (i == map.input_count - 1) ? "" : ",");
        }
        fprintf(f, "  ]\n");
        fprintf(f, "}\n");
        fclose(f);
        printf("\n%s✓ Hardware mapping successfully written to: %s%s\n", COLOR_GREEN, out_path, COLOR_RESET);
    }
}

int main(int argc, char **argv) {
    signal(SIGINT, handle_sigint);
    bool run_wizard = (argc > 1 && strcmp(argv[1], "--map") == 0);

    printf("%s=======================================================%s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s PowerA macOS Multi-Stage Active Hardware Listener    %s\n", COLOR_BOLD, COLOR_RESET);
    printf("%s=======================================================%s\n", COLOR_BOLD, COLOR_RESET);

    // Active detection loop with 5-second hotplug retry
    printf("\nDetecting PowerA controller across hardware stages...\n");
    for (int retry = 0; retry < 5 && !g_stage1.active && !g_stage2.active && g_running; retry++) {
        setup_stage1();
        setup_stage2();
        if (!g_stage1.active && !g_stage2.active) {
            printf("  Waiting for controller connection... (%ds)\n", 5 - retry);
            sleep(1);
        }
    }

    if (g_stage1.active) {
        printf("  %s✓ Stage 1 Connected (IOKit USB Pipe):%s %s (VID: 0x%04X, PID: 0x%04X, InPipe=%d, OutPipe=%d)\n",
               COLOR_GREEN, COLOR_RESET, g_stage1.dev_name, g_stage1.vid, g_stage1.pid, g_stage1.in_pipe, g_stage1.out_pipe);
    } else {
        printf("  %s! Stage 1: No direct USB Interface 0 pipe available.%s\n", COLOR_YELLOW, COLOR_RESET);
    }

    if (g_stage2.active) {
        printf("  %s✓ Stage 2 Connected (HID / Config):%s %s\n", COLOR_GREEN, COLOR_RESET, g_stage2.path);
    } else {
        printf("  %s! Stage 2: No HID endpoints available.%s\n", COLOR_YELLOW, COLOR_RESET);
    }

    if (!g_stage1.active && !g_stage2.active) {
        printf("\n%sError: No PowerA device detected on USB or Bluetooth.%s\n", COLOR_RED, COLOR_RESET);
        printf("Please ensure your controller or 2.4G dongle is plugged in.\n");
        if (g_paused_driver) system("killall -CONT powera-driver 2>/dev/null");
        return 1;
    }

    if (run_wizard) {
        run_mapping_wizard();
    } else {
        printf("\n%s=== Stage 3: Real-Time Bitwise Packet Differ Active ===%s\n", COLOR_BOLD, COLOR_RESET);
        printf("Listening for inputs at 500Hz. Press buttons or move sticks to see changed bytes:\n\n");

        pthread_t t1, t2;
        if (g_stage1.active) pthread_create(&t1, NULL, stage1_thread, &g_stage1);
        if (g_stage2.active) pthread_create(&t2, NULL, stage2_thread, &g_stage2);

        while (g_running) {
            usleep(100000);
        }

        if (g_stage1.active) {
            g_stage1.active = false;
            pthread_join(t1, NULL);
        }
        if (g_stage2.active) {
            g_stage2.active = false;
            pthread_join(t2, NULL);
        }
    }

    printf("\nCleaning up and closing endpoints...\n");
    if (g_stage1.intf) {
        (*g_stage1.intf)->USBInterfaceClose(g_stage1.intf);
        (*g_stage1.intf)->Release(g_stage1.intf);
    }
    if (g_stage2.dev) hid_close(g_stage2.dev);

    if (g_paused_driver) {
        printf("Restoring background driver daemon...\n");
        system("launchctl load ~/Library/LaunchAgents/com.powera.driver.plist 2>/dev/null");
    }
    printf("Done.\n");
    return 0;
}
