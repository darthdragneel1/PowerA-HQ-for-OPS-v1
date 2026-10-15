#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

int main(int argc, char **argv) {
    printf("=== PowerA Input Stream Diagnostic ===\n");
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostInterface");
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS) {
        printf("Failed to get matching services.\n");
        return 1;
    }

    io_service_t service;
    IOUSBInterfaceInterface **intf = NULL;
    UInt8 in_pipe = 0;
    UInt8 out_pipe = 0;

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
                printf("Found PowerA 2.4G Receiver Interface 0!\n");
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
        printf("Could not find or open PowerA interface 0.\n");
        return 1;
    }

    IOReturn ret = (*intf)->USBInterfaceOpen(intf);
    if (ret != KERN_SUCCESS) {
        printf("USBInterfaceOpen failed: 0x%08X (try killing any running driver)\n", ret);
        return 1;
    }
    printf("USB Interface 0 opened successfully!\n");

    UInt8 numEndpoints = 0;
    (*intf)->GetNumEndpoints(intf, &numEndpoints);
    printf("Endpoints count: %d\n", numEndpoints);
    for (UInt8 p = 1; p <= numEndpoints; p++) {
        UInt8 dir = 0, num = 0, type = 0, interval = 0;
        UInt16 maxPkt = 0;
        (*intf)->GetPipeProperties(intf, p, &dir, &num, &type, &maxPkt, &interval);
        printf("Pipe %d: dir=%s type=%s maxPkt=%d interval=%d\n",
               p, (dir == kUSBIn) ? "IN" : "OUT",
               (type == kUSBInterrupt) ? "Interrupt" : (type == kUSBBulk) ? "Bulk" : "Other",
               maxPkt, interval);
        if (dir == kUSBIn && in_pipe == 0) in_pipe = p;
        if (dir == kUSBOut && out_pipe == 0) out_pipe = p;
    }

    // Send standard LED activation command
    if (out_pipe > 0) {
        uint8_t led_cmd[3] = {0x01, 0x03, 0x06};
        IOReturn wret = (*intf)->WritePipe(intf, out_pipe, led_cmd, sizeof(led_cmd));
        printf("Sent LED init packet to Pipe %d: 0x%08X\n", out_pipe, wret);
    }

    printf("Listening for packets on InPipe %d for 5 seconds...\n", in_pipe);
    uint8_t buf[64];
    int pkt_count = 0;
    for (int i = 0; i < 50; i++) {
        UInt32 readSize = sizeof(buf);
        IOReturn r = (*intf)->ReadPipe(intf, in_pipe, buf, &readSize);
        if (r == kIOReturnSuccess && readSize > 0) {
            pkt_count++;
            printf("PKT #%d (len=%u): ", pkt_count, readSize);
            for (UInt32 b = 0; b < readSize; b++) printf("%02X ", buf[b]);
            printf("\n");
        } else if (r == kIOUSBTransactionTimeout) {
            // Timeout - no packet
        } else {
            printf("ReadPipeTO error: 0x%08X\n", r);
            break;
        }
    }
    printf("Total packets received: %d\n", pkt_count);

    (*intf)->USBInterfaceClose(intf);
    (*intf)->Release(intf);
    return 0;
}
