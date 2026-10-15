#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <hidapi/hidapi.h>

int main(int argc, char **argv) {
    int test_mode = 0; // 0 = stop all, 1 = test x360, 2 = test gip, 3 = test hidapi
    uint8_t intensity = 128; // Default 50%
    if (argc > 1) test_mode = atoi(argv[1]);
    if (argc > 2) intensity = (uint8_t)atoi(argv[2]);

    printf("=== PowerA Rumble Diagnostic & Intensity Tester ===\n");
    printf("Mode: %d (0=STOP ALL, 1=X360 Pipe 2, 2=GIP Pipe 2, 3=HIDAPI Intf 1), Intensity=%d\n\n", test_mode, intensity);

    // 1. Direct IOUSBHostInterface match for Interface 0
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostInterface");
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) == KERN_SUCCESS) {
        io_service_t intf_service;
        while ((intf_service = IOIteratorNext(iter))) {
            io_service_t parent = 0;
            IORegistryEntryGetParentEntry(intf_service, kIOServicePlane, &parent);
            if (parent) {
                CFNumberRef vidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(parent, CFSTR("idVendor"), kCFAllocatorDefault, 0);
                CFNumberRef pidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(parent, CFSTR("idProduct"), kCFAllocatorDefault, 0);
                int vid = 0, pid = 0;
                if (vidRef) { CFNumberGetValue(vidRef, kCFNumberIntType, &vid); CFRelease(vidRef); }
                if (pidRef) { CFNumberGetValue(pidRef, kCFNumberIntType, &pid); CFRelease(pidRef); }

                CFNumberRef ifNumRef = (CFNumberRef)IORegistryEntryCreateCFProperty(intf_service, CFSTR("bInterfaceNumber"), kCFAllocatorDefault, 0);
                int ifNum = -1;
                if (ifNumRef) { CFNumberGetValue(ifNumRef, kCFNumberIntType, &ifNum); CFRelease(ifNumRef); }

                if (vid == 0x20D6 && ifNum == 0) {
                    IOCFPlugInInterface **plugIn = NULL;
                    SInt32 score = 0;
                    if (IOCreatePlugInInterfaceForService(intf_service, kIOUSBInterfaceUserClientTypeID,
                                                         kIOCFPlugInInterfaceID, &plugIn, &score) == S_OK && plugIn) {
                        IOUSBInterfaceInterface **intf = NULL;
                        (*plugIn)->QueryInterface(plugIn, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID*)&intf);
                        (*plugIn)->Release(plugIn);
                        if (intf) {
                            IOReturn o_res = (*intf)->USBInterfaceOpenSeize(intf);
                            if (o_res != KERN_SUCCESS) o_res = (*intf)->USBInterfaceOpen(intf);
                            if (o_res == KERN_SUCCESS) {
                                printf("[IOKit Intf 0] Opened successfully.\n");
                                static uint8_t seq = 1;

                                if (test_mode == 0) {
                                    printf("Sending STOP to all protocols on Interface 0...\n");
                                    // X360 Stop
                                    uint8_t x360_stop[8] = {0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                                    (*intf)->WritePipe(intf, 2, x360_stop, 8);
                                    usleep(10000);

                                    // GIP Stop with incremented sequence
                                    uint8_t gip_stop[16] = {0x09, 0x00, seq++, 0x09, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                                    (*intf)->WritePipe(intf, 2, gip_stop, 16);
                                    usleep(10000);
                                    gip_stop[2] = seq++;
                                    (*intf)->WritePipe(intf, 2, gip_stop, 16);
                                } else if (test_mode == 1) {
                                    printf("Testing X360 Rumble (Intensity=%d) for 400ms...\n", intensity);
                                    uint8_t x360[8] = {0x00, 0x08, 0x00, intensity, intensity, 0x00, 0x00, 0x00};
                                    (*intf)->WritePipe(intf, 2, x360, 8);
                                    usleep(400000);
                                    uint8_t x360_stop[8] = {0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                                    (*intf)->WritePipe(intf, 2, x360_stop, 8);
                                    printf("Sent X360 STOP.\n");
                                } else if (test_mode == 2) {
                                    printf("Testing GIP Rumble (Intensity=%d, Duration=400ms)...\n", intensity);
                                    // 40 * 10ms = 400ms duration
                                    uint8_t gip[16] = {0x09, 0x00, seq++, 0x09, 0x00, 0x0F, 0x00, 0x00, intensity, intensity, 40, 0x00, 0x00, 0x00, 0x00, 0x00};
                                    (*intf)->WritePipe(intf, 2, gip, 16);
                                    usleep(450000);
                                    uint8_t gip_stop[16] = {0x09, 0x00, seq++, 0x09, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                                    (*intf)->WritePipe(intf, 2, gip_stop, 16);
                                    printf("Sent GIP STOP.\n");
                                }
                                (*intf)->USBInterfaceClose(intf);
                            }
                            (*intf)->Release(intf);
                        }
                    }
                }
                IOObjectRelease(parent);
            }
            IOObjectRelease(intf_service);
        }
        IOObjectRelease(iter);
    }

    // 2. Test HIDAPI Interface 1
    if (hid_init() == 0) {
        struct hid_device_info *devs = hid_enumerate(0x20D6, 0x0);
        if (devs) {
            hid_device *h = hid_open_path(devs->path);
            if (h) {
                if (test_mode == 0) {
                    printf("[HIDAPI] Sending STOP on Interface 1...\n");
                    uint8_t pkt[64] = {0};
                    pkt[0] = 0x20;
                    pkt[1] = 0x09;
                    pkt[2] = 0x05;
                    pkt[3] = 0x64;
                    pkt[4] = 0x09;
                    pkt[6] = 0x0F;
                    pkt[14] = 0x00; // Off
                    hid_write(h, pkt, 64);
                } else if (test_mode == 3) {
                    printf("[HIDAPI] Testing 0x20 Rumble (Intensity=%d)...\n", intensity);
                    uint8_t pkt[64] = {0};
                    pkt[0] = 0x20;
                    pkt[1] = 0x09;
                    pkt[2] = 0x01;
                    pkt[3] = 0x64;
                    pkt[4] = 0x09;
                    pkt[6] = 0x0F;
                    pkt[7] = 0x20;
                    pkt[8] = 0x20;
                    pkt[9] = intensity;
                    pkt[10] = intensity;
                    pkt[14] = 0xFF;
                    hid_write(h, pkt, 64);
                    usleep(400000);
                    memset(pkt, 0, 64);
                    pkt[0] = 0x20;
                    pkt[1] = 0x09;
                    pkt[2] = 0x02;
                    pkt[3] = 0x64;
                    pkt[4] = 0x09;
                    pkt[6] = 0x0F;
                    pkt[14] = 0x00;
                    hid_write(h, pkt, 64);
                    printf("[HIDAPI] Sent STOP.\n");
                }
                hid_close(h);
            }
            hid_free_enumeration(devs);
        }
        hid_exit();
    }

    printf("Done.\n");
    return 0;
}
