#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

int main(int argc, char **argv) {
    uint8_t intensity = 128;
    int duration_ms = 400;
    if (argc > 1) intensity = (uint8_t)atoi(argv[1]);
    if (argc > 2) duration_ms = atoi(argv[2]);

    printf("=== Dedicated X360 Pipe 2 Rumble Tester ===\n");
    printf("Intensity: %d/255, Duration: %d ms\n", intensity, duration_ms);

    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostInterface");
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS) return 1;

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
                printf("Found PowerA Intf 0 (VID=0x%04X, PID=0x%04X)\n", vid, pid);
                IOCFPlugInInterface **plugIn = NULL;
                SInt32 score = 0;
                if (IOCreatePlugInInterfaceForService(intf_service, kIOUSBInterfaceUserClientTypeID,
                                                     kIOCFPlugInInterfaceID, &plugIn, &score) == S_OK && plugIn) {
                    IOUSBInterfaceInterface **intf = NULL;
                    (*plugIn)->QueryInterface(plugIn, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID*)&intf);
                    (*plugIn)->Release(plugIn);
                    if (intf) {
                        IOReturn open_res = (*intf)->USBInterfaceOpenSeize(intf);
                        if (open_res != KERN_SUCCESS) open_res = (*intf)->USBInterfaceOpen(intf);
                        if (open_res == KERN_SUCCESS) {
                            printf("Connected to Interface 0 Pipe 2!\n");
                            
                            // 1. Send Rumble ON
                            uint8_t rumble_on[8] = {0x00, 0x08, 0x00, intensity, intensity, 0x00, 0x00, 0x00};
                            IOReturn r1 = (*intf)->WritePipe(intf, 2, rumble_on, 8);
                            printf("Sent Rumble ON (res=0x%08X)\n", r1);

                            // Sleep for specified duration
                            usleep(duration_ms * 1000);

                            // 2. Send Rumble OFF (Stop)
                            uint8_t rumble_off[8] = {0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
                            IOReturn r2 = (*intf)->WritePipe(intf, 2, rumble_off, 8);
                            printf("Sent Rumble OFF (res=0x%08X)\n", r2);

                            // Send a second stop packet 10ms later to ensure delivery over 2.4G RF
                            usleep(10000);
                            (*intf)->WritePipe(intf, 2, rumble_off, 8);

                            (*intf)->USBInterfaceClose(intf);
                        } else {
                            printf("Failed to open interface: 0x%08X\n", open_res);
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
    return 0;
}
