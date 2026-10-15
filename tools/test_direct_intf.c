#include <stdio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

int main(void) {
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
                printf("Found PowerA Intf 0! VID=0x%04X, PID=0x%04X\n", vid, pid);
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
                            printf("USBInterfaceOpen returned (0x%08x), trying USBInterfaceOpenSeize...\n", open_res);
                            open_res = (*intf)->USBInterfaceOpenSeize(intf);
                        }
                        if (open_res == KERN_SUCCESS) {
                            printf("SUCCESS! Claimed Interface 0 via direct match!\n");
                            UInt8 numEndpoints = 0;
                            (*intf)->GetNumEndpoints(intf, &numEndpoints);
                            printf("Num Endpoints: %d\n", numEndpoints);
                            for (UInt8 p = 1; p <= numEndpoints; p++) {
                                UInt8 dir = 0, num = 0, type = 0, interval = 0;
                                UInt16 maxPkt = 0;
                                (*intf)->GetPipeProperties(intf, p, &dir, &num, &type, &maxPkt, &interval);
                                printf("Pipe %d: dir=%s, type=%d, maxPkt=%d\n", p, (dir == kUSBIn ? "IN" : "OUT"), type, maxPkt);
                            }
                            (*intf)->USBInterfaceClose(intf);
                        } else {
                            printf("Failed to open interface: 0x%08x\n", open_res);
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
