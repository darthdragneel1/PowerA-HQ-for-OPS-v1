/**
 * PowerA PC HQ for macOS - Native Cocoa / WebKit Application
 * Compiles directly to an authentic native macOS Application Bundle
 */

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <unistd.h>
#import <sys/stat.h>

@interface PowerAAppDelegate : NSObject <NSApplicationDelegate, WKScriptMessageHandler, WKNavigationDelegate>
@property (strong, nonatomic) NSWindow *window;
@property (strong, nonatomic) WKWebView *webView;
@property (strong, nonatomic) NSStatusItem *statusItem;
@property (strong, nonatomic) NSTask *driverTask;
@end

@implementation PowerAAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    // 1. Launch background driver daemon if not already running
    [self launchDriverDaemonIfNeeded];

    // 1b. Set Dock Icon explicitly
    NSString *bundlePath = [[NSBundle mainBundle] resourcePath];
    NSString *iconPath = [bundlePath stringByAppendingPathComponent:@"AppIcon.icns"];
    if (![[NSFileManager defaultManager] fileExistsAtPath:iconPath]) {
        NSString *currentDir = [[NSFileManager defaultManager] currentDirectoryPath];
        iconPath = [currentDir stringByAppendingPathComponent:@"assets/AppIcon.icns"];
        if (![[NSFileManager defaultManager] fileExistsAtPath:iconPath]) {
            NSString *execDir = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
            iconPath = [[execDir stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"Resources/AppIcon.icns"];
        }
    }
    NSImage *appIcon = [[NSImage alloc] initWithContentsOfFile:iconPath];
    if (appIcon) {
        [NSApp setApplicationIconImage:appIcon];
    }

    // 2. Configure Native Window
    NSRect frame = NSMakeRect(100, 100, 1180, 780);
    NSWindowStyleMask styleMask = NSWindowStyleMaskTitled |
                                  NSWindowStyleMaskClosable |
                                  NSWindowStyleMaskMiniaturizable |
                                  NSWindowStyleMaskResizable |
                                  NSWindowStyleMaskFullSizeContentView;

    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:styleMask
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];

    [self.window setTitle:@"PowerA PC HQ"];
    [self.window setTitlebarAppearsTransparent:YES];
    [self.window setTitleVisibility:NSWindowTitleHidden];
    [self.window setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameDarkAqua]];
    [self.window setBackgroundColor:[NSColor colorWithCalibratedRed:0.055 green:0.055 blue:0.067 alpha:1.0]];
    [self.window center];

    // 3. Configure WebKit & Custom Script Handlers
    WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
    WKUserContentController *controller = [[WKUserContentController alloc] init];
    [controller addScriptMessageHandler:self name:@"windowClose"];
    [controller addScriptMessageHandler:self name:@"windowMinimize"];
    [controller addScriptMessageHandler:self name:@"windowZoom"];
    config.userContentController = controller;

    self.webView = [[WKWebView alloc] initWithFrame:[self.window.contentView bounds] configuration:config];
    [self.webView setAutoresizingMask:(NSViewWidthSizable | NSViewHeightSizable)];
    [self.webView setValue:@NO forKey:@"drawsBackground"];
    self.webView.navigationDelegate = self;

    [self.window.contentView addSubview:self.webView];

    // 4. Load Application HTML
    NSString *htmlPath = [bundlePath stringByAppendingPathComponent:@"ui/index.html"];
    
    // Fallback to relative directory if running standalone binary
    if (![[NSFileManager defaultManager] fileExistsAtPath:htmlPath]) {
        NSString *currentDir = [[NSFileManager defaultManager] currentDirectoryPath];
        htmlPath = [currentDir stringByAppendingPathComponent:@"src/ui/index.html"];
        if (![[NSFileManager defaultManager] fileExistsAtPath:htmlPath]) {
            NSString *execDir = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
            htmlPath = [[execDir stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"Resources/ui/index.html"];
        }
    }

    NSURL *url = [NSURL fileURLWithPath:htmlPath];
    NSURL *readAccessUrl = [url URLByDeletingLastPathComponent];
    // Grant access to parent directory so assets can be loaded
    readAccessUrl = [[readAccessUrl URLByDeletingLastPathComponent] URLByDeletingLastPathComponent];
    [self.webView loadFileURL:url allowingReadAccessToURL:readAccessUrl];

    // 5. Setup Menu Bar Item
    [self setupStatusItem];

    // 6. Present Window
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)setupStatusItem {
    self.statusItem = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
    NSImage *icon = [NSImage imageWithSystemSymbolName:@"gamecontroller.fill" accessibilityDescription:@"PowerA"];
    if (icon) {
        [icon setTemplate:YES];
        self.statusItem.button.image = icon;
    } else {
        self.statusItem.button.title = @"PowerA";
    }

    NSMenu *menu = [[NSMenu alloc] init];
    [menu addItemWithTitle:@"PowerA OPS v1 (No Device Connected)" action:nil keyEquivalent:@""];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:@"Open PowerA PC HQ" action:@selector(showMainWindow) keyEquivalent:@"o"];
    [menu addItemWithTitle:@"T1 Profile (Default)" action:nil keyEquivalent:@"1"];
    [menu addItemWithTitle:@"T2 Profile (Speed)" action:nil keyEquivalent:@"2"];
    [menu addItemWithTitle:@"T3 Profile (Hair Trigger)" action:nil keyEquivalent:@"3"];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItemWithTitle:@"Quit PowerA PC HQ" action:@selector(quitApp) keyEquivalent:@"q"];
    self.statusItem.menu = menu;
}

- (void)showMainWindow {
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)quitApp {
    [NSApp terminate:nil];
}

- (void)launchDriverDaemonIfNeeded {
    // Check if driver daemon is already responding on 127.0.0.1:8765
    NSURL *url = [NSURL URLWithString:@"http://127.0.0.1:8765/api/status"];
    NSMutableURLRequest *req = [NSMutableURLRequest requestWithURL:url];
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block NSInteger statusCode = 0;
    [[[NSURLSession sharedSession] dataTaskWithRequest:req completionHandler:^(NSData *data, NSURLResponse *resp, NSError *error) {
        (void)data; (void)error;
        if ([resp isKindOfClass:[NSHTTPURLResponse class]]) {
            statusCode = [(NSHTTPURLResponse *)resp statusCode];
        }
        dispatch_semaphore_signal(sem);
    }] resume];
    dispatch_semaphore_wait(sem, dispatch_time(DISPATCH_TIME_NOW, (int64_t)(300 * NSEC_PER_MSEC)));
    if (statusCode == 200) {
        NSLog(@"[PowerA App] Background driver daemon is already active on port 8765.");
        return;
    }

    NSString *bundleDriver = [[NSBundle mainBundle] pathForAuxiliaryExecutable:@"powera-driver"];
    if (!bundleDriver || ![[NSFileManager defaultManager] isExecutableFileAtPath:bundleDriver]) {
        bundleDriver = [[[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"Contents/MacOS"] stringByAppendingPathComponent:@"powera-driver"];
    }
    NSString *driverBin = bundleDriver;
    if (![[NSFileManager defaultManager] isExecutableFileAtPath:driverBin]) {
        NSString *currentDir = [[NSFileManager defaultManager] currentDirectoryPath];
        driverBin = [currentDir stringByAppendingPathComponent:@"bin/powera-driver"];
        if (![[NSFileManager defaultManager] isExecutableFileAtPath:driverBin]) {
            NSString *execDir = [[[NSBundle mainBundle] executablePath] stringByDeletingLastPathComponent];
            driverBin = [execDir stringByAppendingPathComponent:@"powera-driver"];
            if (![[NSFileManager defaultManager] isExecutableFileAtPath:driverBin]) {
                driverBin = @"/usr/local/bin/powera-driver";
            }
        }
    }
    if ([[NSFileManager defaultManager] isExecutableFileAtPath:driverBin]) {
        self.driverTask = [[NSTask alloc] init];
        self.driverTask.executableURL = [NSURL fileURLWithPath:driverBin];
        self.driverTask.arguments = @[@"--daemon"];
        @try {
            [self.driverTask launch];
            NSLog(@"[PowerA App] Started background driver daemon successfully.");
        } @catch (NSException *e) {
            NSLog(@"[PowerA App] Note: Could not launch separate daemon: %@", e);
        }
    }
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    if ([message.name isEqualToString:@"windowClose"]) {
        [self.window performClose:nil];
    } else if ([message.name isEqualToString:@"windowMinimize"]) {
        [self.window miniaturize:nil];
    } else if ([message.name isEqualToString:@"windowZoom"]) {
        [self.window zoom:nil];
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return NO; // Keep running in menu bar
}

- (void)applicationWillTerminate:(NSNotification *)notification {
    if (self.driverTask && [self.driverTask isRunning]) {
        [self.driverTask terminate];
    }
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Standard App Menus
        NSMenu *menubar = [[NSMenu alloc] init];
        NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        [app setMainMenu:menubar];

        NSMenu *appMenu = [[NSMenu alloc] init];
        [appMenu addItemWithTitle:@"About PowerA PC HQ" action:@selector(orderFrontStandardAboutPanel:) keyEquivalent:@""];
        [appMenu addItem:[NSMenuItem separatorItem]];
        [appMenu addItemWithTitle:@"Hide PowerA PC HQ" action:@selector(hide:) keyEquivalent:@"h"];
        [appMenu addItemWithTitle:@"Quit PowerA PC HQ" action:@selector(terminate:) keyEquivalent:@"q"];
        [appMenuItem setSubmenu:appMenu];

        PowerAAppDelegate *delegate = [[PowerAAppDelegate alloc] init];
        [app setDelegate:delegate];
        [app run];
    }
    return 0;
}
