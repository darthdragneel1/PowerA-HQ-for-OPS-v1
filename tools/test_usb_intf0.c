#include <stdio.h>
#include <stdlib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

int main(void) {
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostDevice");
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS) {
        printf("Failed to get matching USB devices\n");
        return 1;
    }

    io_service_t device;
    int found = 0;
    while ((device = IOIteratorNext(iter))) {
        CFNumberRef vidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(device, CFSTR("idVendor"), kCFAllocatorDefault, 0);
        CFNumberRef pidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(device, CFSTR("idProduct"), kCFAllocatorDefault, 0);
        int vid = 0, pid = 0;
        if (vidRef) { CFNumberGetValue(vidRef, kCFNumberIntType, &vid); CFRelease(vidRef); }
        if (pidRef) { CFNumberGetValue(pidRef, kCFNumberIntType, &pid); CFRelease(pidRef); }

        CFStringRef prodRef = (CFStringRef)IORegistryEntryCreateCFProperty(device, CFSTR("USB Product Name"), kCFAllocatorDefault, 0);
        char prodName[128] = "Unknown";
        if (prodRef) {
            CFStringGetCString(prodRef, prodName, sizeof(prodName), kCFStringEncodingUTF8);
            CFRelease(prodRef);
        }

        if (vid == 0x20D6 || vid == 0x045E || vid == 0x24C6 || vid == 0x2E2C) {
            found++;
            printf("Found Game Controller: %s (VID: 0x%04X, PID: 0x%04X)\n", prodName, vid, pid);

            io_iterator_t childIter;
            IORegistryEntryGetChildIterator(device, kIOServicePlane, &childIter);
            io_service_t child;
            while ((child = IOIteratorNext(childIter))) {
                CFNumberRef ifNumRef = (CFNumberRef)IORegistryEntryCreateCFProperty(child, CFSTR("bInterfaceNumber"), kCFAllocatorDefault, 0);
                CFNumberRef ifClassRef = (CFNumberRef)IORegistryEntryCreateCFProperty(child, CFSTR("bInterfaceClass"), kCFAllocatorDefault, 0);
                CFNumberRef ifSubClassRef = (CFNumberRef)IORegistryEntryCreateCFProperty(child, CFSTR("bInterfaceSubClass"), kCFAllocatorDefault, 0);
                int ifNum = -1, ifClass = -1, ifSubClass = -1;
                if (ifNumRef) { CFNumberGetValue(ifNumRef, kCFNumberIntType, &ifNum); CFRelease(ifNumRef); }
                if (ifClassRef) { CFNumberGetValue(ifClassRef, kCFNumberIntType, &ifClass); CFRelease(ifClassRef); }
                if (ifSubClassRef) { CFNumberGetValue(ifSubClassRef, kCFNumberIntType, &ifSubClass); CFRelease(ifSubClassRef); }

                printf("  -> Interface %d: Class=0x%02X, SubClass=0x%02X\n", ifNum, ifClass, ifSubClass);
                IOObjectRelease(child);
            }
            IOObjectRelease(childIter);
        }
        IOObjectRelease(device);
    }
    IOObjectRelease(iter);
    if (!found) {
        printf("No PowerA (0x20D6) or Xbox (0x045E) devices currently enumerated on USB bus.\n");
    }
    return 0;
}
