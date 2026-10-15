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
    printf("=== PowerA Live Hardware Diagnostics ===\n");
    if (hid_init() != 0) {
        printf("hid_init failed!\n");
        return 1;
    }

    struct hid_device_info *devs = hid_enumerate(0x20D6, 0x0);
    if (!devs) devs = hid_enumerate(0x24C6, 0x0);
    if (!devs) devs = hid_enumerate(0x2E2C, 0x0);

    if (!devs) {
        printf("No HID device enumerated for 0x20D6 / 0x24C6 / 0x2E2C\n");
    } else {
        struct hid_device_info *cur = devs;
        while (cur) {
            printf("Found HID Dev: VID=0x%04X PID=0x%04X Path=%s UsagePage=0x%04X Usage=0x%04X\n",
                   cur->vendor_id, cur->product_id, cur->path, cur->usage_page, cur->usage);
            cur = cur->next;
        }
    }

    // Try opening the first HID device
    if (devs) {
        hid_device *h = hid_open_path(devs->path);
        if (!h) {
            printf("hid_open_path failed! Trying hid_open(%04x, %04x)...\n", devs->vendor_id, devs->product_id);
            h = hid_open(devs->vendor_id, devs->product_id, NULL);
        }
        if (h) {
            printf("Successfully opened HID device (%s)!\n", devs->path);
            
            // Try reading feature report
            uint8_t feat[64] = {0};
            feat[0] = 0xE1;
            int fr = hid_get_feature_report(h, feat, sizeof(feat));
            printf("hid_get_feature_report(0xE1) returned: %d\n", fr);
            
            // Try a 0xE1 ping write
            uint8_t ping[64] = {0xE1, 0x00, 0x01, 0x3C, 0x20, 0x00};
            int wr = hid_write(h, ping, sizeof(ping));
            printf("hid_write(ping) returned: %d\n", wr);
            
            // Try reading input
            uint8_t in_buf[64] = {0};
            int rd = hid_read_timeout(h, in_buf, sizeof(in_buf), 200);
            printf("hid_read_timeout returned: %d\n", rd);
            if (rd > 0) {
                printf("HID Data: ");
                for (int i = 0; i < rd && i < 16; i++) printf("%02X ", in_buf[i]);
                printf("\n");
            }
            hid_close(h);
        } else {
            printf("Failed to open HID device.\n");
        }
        hid_free_enumeration(devs);
    }

    // Now test IOKit on Interface 0
    printf("\n--- Testing IOKit on Interface 0 ---\n");
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

            if (vid == 0x20D6 || vid == 0x24C6) {
                printf("IOKit found device VID: 0x%04X, PID: 0x%04X\n", vid, pid);
                io_iterator_t childIter;
                IORegistryEntryGetChildIterator(device, kIOServicePlane, &childIter);
                io_service_t child;
                while ((child = IOIteratorNext(childIter))) {
                    CFNumberRef ifNumRef = (CFNumberRef)IORegistryEntryCreateCFProperty(child, CFSTR("bInterfaceNumber"), kCFAllocatorDefault, 0);
                    int ifNum = -1;
                    if (ifNumRef) { CFNumberGetValue(ifNumRef, kCFNumberIntType, &ifNum); CFRelease(ifNumRef); }
                    if (ifNum == 0) {
                        printf("Found Interface 0! Attempting to open user client...\n");
                        IOCFPlugInInterface **plugIn = NULL;
                        SInt32 score = 0;
                        HRESULT res = IOCreatePlugInInterfaceForService(child, kIOUSBInterfaceUserClientTypeID,
                                                                        kIOCFPlugInInterfaceID, &plugIn, &score);
                        if (res == S_OK && plugIn) {
                            IOUSBInterfaceInterface **intf = NULL;
                            (*plugIn)->QueryInterface(plugIn, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID*)&intf);
                            (*plugIn)->Release(plugIn);
                            if (intf) {
                                IOReturn openRes = (*intf)->USBInterfaceOpen(intf);
                                printf("USBInterfaceOpen returned: 0x%08X (%s)\n",
                                       openRes, openRes == KERN_SUCCESS ? "SUCCESS" : "FAILED / BUSY");
                                if (openRes == KERN_SUCCESS) {
                                    UInt8 numEndpoints = 0;
                                    (*intf)->GetNumEndpoints(intf, &numEndpoints);
                                    printf("Number of endpoints on Interface 0: %d\n", numEndpoints);
                                    for (UInt8 p = 1; p <= numEndpoints; p++) {
                                        UInt8 dir = 0, num = 0, type = 0, interval = 0;
                                        UInt16 maxPkt = 0;
                                        (*intf)->GetPipeProperties(intf, p, &dir, &num, &type, &maxPkt, &interval);
                                        printf("  Pipe %d: dir=%s, transferType=%d, maxPacketSize=%d\n",
                                               p, dir == kUSBIn ? "IN" : "OUT", type, maxPkt);
                                        if (dir == kUSBIn) {
                                            uint8_t buf[64] = {0};
                                            UInt32 sz = sizeof(buf);
                                            IOReturn r = (*intf)->ReadPipeTO(intf, p, buf, &sz, 200, 200);
                                            printf("  -> ReadPipeTO on Pipe %d returned: 0x%08X (bytes read: %u)\n", p, (unsigned int)r, (unsigned int)sz);
                                            if (r == kIOReturnSuccess && sz > 0) {
                                                printf("  Data: ");
                                                for (UInt32 i = 0; i < sz && i < 20; i++) printf("%02X ", buf[i]);
                                                printf("\n");
                                            }
                                        }
                                    }
                                    (*intf)->USBInterfaceClose(intf);
                                }
                                (*intf)->Release(intf);
                            }
                        } else {
                            printf("IOCreatePlugInInterfaceForService failed: 0x%08X\n", (uint32_t)res);
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
