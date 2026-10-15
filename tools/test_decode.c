#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

int main() {
    printf("Connecting to PowerA USB Interface 0...\n");
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostInterface");
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS) return 1;

    io_service_t service;
    IOUSBInterfaceInterface **intf = NULL;
    UInt8 in_pipe = 0;

    while ((service = IOIteratorNext(iter))) {
        io_service_t parent = 0;
        IORegistryEntryGetParentEntry(service, kIOServicePlane, &parent);
        if (parent) {
            CFNumberRef vidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(parent, CFSTR("idVendor"), kCFAllocatorDefault, 0);
            CFNumberRef pidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(parent, CFSTR("idProduct"), kCFAllocatorDefault, 0);
            int vid = 0, pid = 0;
            if (vidRef) { CFNumberGetValue(vidRef, kCFNumberIntType, &vid); CFRelease(vidRef); }
            if (pidRef) { CFNumberGetValue(pidRef, kCFNumberIntType, &pid); CFRelease(pidRef); }

            CFNumberRef ifNumRef = (CFNumberRef)IORegistryEntryCreateCFProperty(service, CFSTR("bInterfaceNumber"), kCFAllocatorDefault, 0);
            int ifNum = -1;
            if (ifNumRef) { CFNumberGetValue(ifNumRef, kCFNumberIntType, &ifNum); CFRelease(ifNumRef); }

            if (vid == 0x20D6 && pid == 0x4027 && ifNum == 0) {
                IOCFPlugInInterface **plugIn = NULL;
                SInt32 score = 0;
                if (IOCreatePlugInInterfaceForService(service, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plugIn, &score) == S_OK) {
                    (*plugIn)->QueryInterface(plugIn, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID*)&intf);
                    (*plugIn)->Release(plugIn);
                }
                IOObjectRelease(service);
                IOObjectRelease(parent);
                break;
            }
            IOObjectRelease(parent);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iter);

    if (!intf) {
        printf("Interface not found.\n");
        return 1;
    }

    if ((*intf)->USBInterfaceOpen(intf) != KERN_SUCCESS) {
        printf("Could not open interface.\n");
        return 1;
    }

    UInt8 numEndpoints = 0;
    (*intf)->GetNumEndpoints(intf, &numEndpoints);
    for (UInt8 p = 1; p <= numEndpoints; p++) {
        UInt8 dir = 0, num = 0, type = 0, interval = 0;
        UInt16 maxPkt = 0;
        (*intf)->GetPipeProperties(intf, p, &dir, &num, &type, &maxPkt, &interval);
        if (dir == kUSBIn && in_pipe == 0) in_pipe = p;
    }

    printf("Reading 10 live packets...\n");
    uint8_t buf[64];
    for (int i = 0; i < 10; i++) {
        UInt32 readSize = sizeof(buf);
        IOReturn r = (*intf)->ReadPipe(intf, in_pipe, buf, &readSize);
        if (r == KERN_SUCCESS && readSize >= 14) {
            uint8_t d1 = buf[2];
            uint8_t d2 = buf[3];
            uint8_t lt = buf[4];
            uint8_t rt = buf[5];
            int16_t lx = (int16_t)((uint16_t)buf[6] | ((uint16_t)buf[7] << 8));
            int16_t ly = (int16_t)((uint16_t)buf[8] | ((uint16_t)buf[9] << 8));
            int16_t rx = (int16_t)((uint16_t)buf[10] | ((uint16_t)buf[11] << 8));
            int16_t ry = (int16_t)((uint16_t)buf[12] | ((uint16_t)buf[13] << 8));
            uint8_t extra = (readSize >= 15) ? buf[14] : 0;

            printf("[Pkt %d] LT=%3u RT=%3u | LX=%6d LY=%6d | RX=%6d RY=%6d | D1=0x%02X D2=0x%02X Extra=0x%02X\n",
                   i, lt, rt, lx, ly, rx, ry, d1, d2, extra);
        }
    }

    (*intf)->USBInterfaceClose(intf);
    (*intf)->Release(intf);
    return 0;
}
