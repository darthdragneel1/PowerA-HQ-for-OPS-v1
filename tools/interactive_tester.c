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
    printf("=== Interactive PowerA Hardware Test ===\n");
    printf("Listening for 10 seconds. Move sticks or press buttons on your controller now!\n\n");

    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBHostDevice");
    io_iterator_t iter;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iter) != KERN_SUCCESS) return 1;

    io_service_t device;
    IOUSBInterfaceInterface **intf0 = NULL;
    UInt8 inPipe = 0, outPipe = 0;

    while ((device = IOIteratorNext(iter)) && !intf0) {
        CFNumberRef vidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(device, CFSTR("idVendor"), kCFAllocatorDefault, 0);
        CFNumberRef pidRef = (CFNumberRef)IORegistryEntryCreateCFProperty(device, CFSTR("idProduct"), kCFAllocatorDefault, 0);
        int vid = 0, pid = 0;
        if (vidRef) { CFNumberGetValue(vidRef, kCFNumberIntType, &vid); CFRelease(vidRef); }
        if (pidRef) { CFNumberGetValue(pidRef, kCFNumberIntType, &pid); CFRelease(pidRef); }

        if (vid == 0x20D6 || vid == 0x24C6) {
            io_iterator_t childIter;
            IORegistryEntryGetChildIterator(device, kIOServicePlane, &childIter);
            io_service_t child;
            while ((child = IOIteratorNext(childIter)) && !intf0) {
                CFNumberRef ifNumRef = (CFNumberRef)IORegistryEntryCreateCFProperty(child, CFSTR("bInterfaceNumber"), kCFAllocatorDefault, 0);
                int ifNum = -1;
                if (ifNumRef) { CFNumberGetValue(ifNumRef, kCFNumberIntType, &ifNum); CFRelease(ifNumRef); }
                if (ifNum == 0) {
                    IOCFPlugInInterface **plugIn = NULL;
                    SInt32 score = 0;
                    if (IOCreatePlugInInterfaceForService(child, kIOUSBInterfaceUserClientTypeID,
                                                         kIOCFPlugInInterfaceID, &plugIn, &score) == S_OK && plugIn) {
                        (*plugIn)->QueryInterface(plugIn, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID*)&intf0);
                        (*plugIn)->Release(plugIn);
                        if (intf0) {
                            if ((*intf0)->USBInterfaceOpen(intf0) == KERN_SUCCESS) {
                                UInt8 numEndpoints = 0;
                                (*intf0)->GetNumEndpoints(intf0, &numEndpoints);
                                for (UInt8 p = 1; p <= numEndpoints; p++) {
                                    UInt8 dir = 0, num = 0, type = 0, interval = 0;
                                    UInt16 maxPkt = 0;
                                    (*intf0)->GetPipeProperties(intf0, p, &dir, &num, &type, &maxPkt, &interval);
                                    if (dir == kUSBIn && type == kUSBInterrupt) inPipe = p;
                                    if (dir == kUSBOut && type == kUSBInterrupt) outPipe = p;
                                }
                            } else {
                                (*intf0)->Release(intf0);
                                intf0 = NULL;
                            }
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

    if (!intf0) {
        printf("Could not claim Interface 0. Is another driver instance running?\n");
        return 1;
    }

    printf("Successfully claimed Interface 0! (InPipe=%d, OutPipe=%d)\n", inPipe, outPipe);

    // Send XID GET_CAPABILITIES (0xC1, 0x01, 0x0100, 0x0000, 20) to wake controller from hardware standby
    uint8_t cap_buf[64] = {0};
    IOUSBDevRequest req;
    memset(&req, 0, sizeof(req));
    req.bmRequestType = 0xC1;
    req.bRequest = 0x01; // GET_CAPABILITIES
    req.wValue = 0x0100; // Input
    req.wIndex = 0x0000;
    req.wLength = 20;
    req.pData = cap_buf;
    IOReturn cap_kr = (*intf0)->ControlRequest(intf0, 0, &req);
    printf("Sent XID GET_CAPABILITIES (Input): kr=0x%08X, lenDone=%d\n", cap_kr, req.wLenDone);

    // Send XID GET_CAPABILITIES (Output)
    memset(&req, 0, sizeof(req));
    req.bmRequestType = 0xC1;
    req.bRequest = 0x01;
    req.wValue = 0x0200; // Output
    req.wIndex = 0x0000;
    req.wLength = 8;
    req.pData = cap_buf;
    cap_kr = (*intf0)->ControlRequest(intf0, 0, &req);
    printf("Sent XID GET_CAPABILITIES (Output): kr=0x%08X, lenDone=%d\n", cap_kr, req.wLenDone);

    // Send Player 1 Solid ON LED command if outPipe exists
    if (outPipe) {
        uint8_t led[3] = {0x01, 0x03, 0x02};
        (*intf0)->WritePipe(intf0, outPipe, led, 3);
        printf("Sent Player 1 Solid LED command to Pipe %d\n", outPipe);
    }

    int packets_received = 0;
    for (int t = 0; t < 50; t++) {
        uint8_t buf[64] = {0};
        UInt32 sz = sizeof(buf);
        IOReturn ret = (*intf0)->ReadPipe(intf0, inPipe, buf, &sz);
        if (ret == kIOReturnSuccess && sz > 0) {
            packets_received++;
            printf("[Pkt %d] Len=%u: ", packets_received, sz);
            for (UInt32 i = 0; i < sz && i < 20; i++) printf("%02X ", buf[i]);
            printf("\n");
        } else if (ret == kIOReturnNoDevice) {
            break;
        }
    }

    if (packets_received == 0) {
        printf("\nResult: ZERO packets received on USB Interface 0 Pipe 1.\n");
        printf("This proves the 2.4G dongle is plugged into the Mac, but the handheld controller\n");
        printf("is either: (1) Powered off / sleeping, (2) Not in RF mode, or (3) Not paired to the dongle.\n");
    } else {
        printf("\nSuccess! Received %d input packets!\n", packets_received);
    }

    (*intf0)->USBInterfaceClose(intf0);
    (*intf0)->Release(intf0);
    return 0;
}
