#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hidapi/hidapi.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

int main(void) {
    printf("=== Testing Rumble on PowerA Hardware ===\n");
    if (hid_init() != 0) return 1;

    // 1. Test Interface 1 HID Rumble
    struct hid_device_info *devs = hid_enumerate(0x20D6, 0x0);
    if (devs) {
        hid_device *h = hid_open_path(devs->path);
        if (h) {
            printf("[HID] Opened %s\n", devs->path);
            
            // Format A: 64-byte raw 0x20 packet
            uint8_t pkt_a[64] = {0};
            pkt_a[0] = 0x20;
            pkt_a[1] = 0x09;
            pkt_a[3] = 0x64;
            pkt_a[4] = 0x09;
            pkt_a[6] = 0x0F;
            pkt_a[7] = 0x32;
            pkt_a[8] = 0x32;
            pkt_a[9] = 200;  // Left grip motor (strong)
            pkt_a[10] = 200; // Right grip motor (strong)
            pkt_a[11] = 200; // Left trigger motor
            pkt_a[12] = 200; // Right trigger motor
            pkt_a[14] = 0xFF;
            int r_a = hid_write(h, pkt_a, 64);
            printf("[HID] Sent 64-byte 0x20 packet, res=%d\n", r_a);

            // Format B: 15-byte packet
            int r_b = hid_write(h, pkt_a, 15);
            printf("[HID] Sent 15-byte 0x20 packet, res=%d\n", r_b);
            
            // Format C: 0xE1 packet
            uint8_t pkt_c[64] = {0};
            pkt_c[0] = 0xE1;
            pkt_c[2] = 0x01;
            pkt_c[3] = 0x3C;
            pkt_c[4] = 0x20; // Opcode
            pkt_c[5] = 200;
            pkt_c[6] = 200;
            pkt_c[7] = 200;
            pkt_c[8] = 200;
            int r_c = hid_write(h, pkt_c, 64);
            printf("[HID] Sent 64-byte 0xE1 packet, res=%d\n", r_c);

            hid_close(h);
        }
        hid_free_enumeration(devs);
    }

    // 2. Test Interface 0 XInput Rumble via Pipe 2
    printf("\n[IOKit] Finding Interface 0...\n");
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostDevice");
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) == KERN_SUCCESS) {
        io_service_t device;
        while ((device = IOIteratorNext(iter))) {
            CFNumberRef vidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(device, CFSTR("idVendor"), kCFAllocatorDefault, 0);
            CFNumberRef pidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(device, CFSTR("idProduct"), kCFAllocatorDefault, 0);
            int vid = 0, pid = 0;
            if (vidRef) { CFNumberGetValue(vidRef, kCFNumberIntType, &vid); CFRelease(vidRef); }
            if (pidRef) { CFNumberGetValue(pidRef, kCFNumberIntType, &pid); CFRelease(pidRef); }

            if (vid == 0x20D6) {
                io_iterator_t childIter;
                IORegistryEntryGetChildIterator(device, kIOServicePlane, &childIter);
                io_service_t child;
                while ((child = IOIteratorNext(childIter))) {
                    CFNumberRef ifNumRef = (CFNumberRef)IORegistryEntryCreateCFProperty(child, CFSTR("bInterfaceNumber"), kCFAllocatorDefault, 0);
                    int ifNum = -1;
                    if (ifNumRef) { CFNumberGetValue(ifNumRef, kCFNumberIntType, &ifNum); CFRelease(ifNumRef); }
                    if (ifNum == 0) {
                        IOCFPlugInInterface **plugIn = NULL;
                        SInt32 score = 0;
                        if (IOCreatePlugInInterfaceForService(child, kIOUSBInterfaceUserClientTypeID,
                                                             kIOCFPlugInInterfaceID, &plugIn, &score) == S_OK && plugIn) {
                            IOUSBInterfaceInterface **intf = NULL;
                            (*plugIn)->QueryInterface(plugIn, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID*)&intf);
                            (*plugIn)->Release(plugIn);
                            if (intf) {
                                if ((*intf)->USBInterfaceOpen(intf) == KERN_SUCCESS) {
                                    printf("[IOKit] Opened Interface 0! Sending standard Xbox 360 rumble...\n");
                                    // Standard Xbox 360 controller rumble packet (8 bytes):
                                    // Byte 0: 0x00
                                    // Byte 1: 0x08 (length)
                                    // Byte 2: 0x00
                                    // Byte 3: left motor (heavy/grip) 0-255
                                    // Byte 4: right motor (light/grip) 0-255
                                    // Byte 5-7: 0x00
                                    uint8_t xbox_rumble[8] = {0x00, 0x08, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00};
                                    IOReturn wr1 = (*intf)->WritePipe(intf, 2, xbox_rumble, sizeof(xbox_rumble));
                                    printf("[IOKit] WritePipe(2, xbox_rumble) returned: 0x%08X\n", wr1);

                                    // Also send Xbox One rumble (if GIP):
                                    // Byte 0: 0x09 (Rumble)
                                    // Byte 1: 0x00
                                    // Byte 2: 0x00
                                    // Byte 3: 0x09 (Size)
                                    // Byte 4: 0x00 (Mode)
                                    // Byte 5: 0x0F (All 4 motors)
                                    // Byte 6: Left Trigger
                                    // Byte 7: Right Trigger
                                    // Byte 8: Left Grip
                                    // Byte 9: Right Grip
                                    // Byte 10: Duration (0xFF = continuous, or 0x64)
                                    uint8_t gip_rumble[16] = {0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x64, 0x64, 0xFF, 0xFF, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00};
                                    IOReturn wr2 = (*intf)->WritePipe(intf, 2, gip_rumble, sizeof(gip_rumble));
                                    printf("[IOKit] WritePipe(2, gip_rumble) returned: 0x%08X\n", wr2);

                                    usleep(500000); // 500ms

                                    // Turn off rumble
                                    uint8_t xbox_stop[8] = {0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                                    (*intf)->WritePipe(intf, 2, xbox_stop, sizeof(xbox_stop));

                                    (*intf)->USBInterfaceClose(intf);
                                }
                                (*intf)->Release(intf);
                            }
                        }
                    }
                    IOObjectRelease(child);
                }
                IOObjectRelease(childIter);
            }
            IOObjectRelease(device);
        }
        IOObjectRelease(iter);
    }
    return 0;
}
