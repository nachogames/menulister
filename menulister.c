#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

// Constants for notifications
#define kAXFocusedApplicationChangedNotification CFSTR("AXFocusedApplicationChanged")
#define kAXFocusedWindowChangedNotification CFSTR("AXFocusedWindowChanged")

// Global run loop reference for clean shutdown
static CFRunLoopRef gMainRunLoop = NULL;

static void cleanup(void) {
    if (gMainRunLoop) {
        CFRunLoopStop(gMainRunLoop);
    }
}

static void signal_handler(int signum) {
    printf("\nReceived signal %d, shutting down...\n", signum);
    cleanup();
}

static void printMenuHierarchy(AXUIElementRef menuElement, int indent) {
    if (!menuElement) return;

    // Get the item's title
    CFTypeRef title = NULL;
    AXError err = AXUIElementCopyAttributeValue(menuElement, kAXTitleAttribute, &title);
    
    if (err == kAXErrorSuccess && title) {
        // Print indent + title
        for (int i = 0; i < indent; i++) {
            printf("  ");
        }
        
        if (CFGetTypeID(title) == CFStringGetTypeID()) {
            CFStringRef titleString = (CFStringRef)title;
            // Use dynamic allocation for very long strings
            CFIndex maxLength = CFStringGetMaximumSizeForEncoding(
                CFStringGetLength(titleString), 
                kCFStringEncodingUTF8
            ) + 1;
            
            char *buffer = (char *)malloc(maxLength);
            if (buffer) {
                if (CFStringGetCString(titleString, buffer, maxLength, kCFStringEncodingUTF8)) {
                    printf("- %s\n", buffer);
                } else {
                    printf("- (unable to read title)\n");
                }
                free(buffer);
            } else {
                printf("- (memory allocation failed)\n");
            }
        } else {
            printf("- (invalid title type)\n");
        }
        
        CFRelease(title);
    } else {
        for (int i = 0; i < indent; i++) printf("  ");
        printf("- (no title)\n");
    }

    // Get children (submenus)
    CFTypeRef children = NULL;
    err = AXUIElementCopyAttributeValue(menuElement, kAXChildrenAttribute, &children);
    
    if (err == kAXErrorSuccess && children) {
        if (CFGetTypeID(children) == CFArrayGetTypeID()) {
            CFArrayRef childrenArray = (CFArrayRef)children;
            CFIndex count = CFArrayGetCount(childrenArray);
            for (CFIndex i = 0; i < count; i++) {
                AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(childrenArray, i);
                printMenuHierarchy(child, indent + 1);
            }
        }
        CFRelease(children);
    }
}

static Boolean checkAccessibilityPermissions(void) {
    // Create options dictionary
    CFStringRef keys[] = { kAXTrustedCheckOptionPrompt };
    CFBooleanRef values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault,
        (const void **)&keys,
        (const void **)&values,
        1,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    
    Boolean trusted = AXIsProcessTrustedWithOptions(options);
    if (options) CFRelease(options);
    return trusted;
}

static void listMenusOfFrontmostApp(void) {
    ProcessSerialNumber psn;
    OSErr err = GetFrontProcess(&psn);
    if (err != noErr) {
        printf("[!] Could not get front process. Error: %d\n", err);
        return;
    }

    pid_t pid;
    err = GetProcessPID(&psn, &pid);
    if (err != noErr) {
        printf("[!] Could not get process PID. Error: %d\n", err);
        return;
    }

    AXUIElementRef app = AXUIElementCreateApplication(pid);
    if (!app) {
        printf("[!] Could not create application reference.\n");
        return;
    }

    AXUIElementRef menuBar = NULL;
    AXError axErr = AXUIElementCopyAttributeValue(
        app, 
        kAXMenuBarAttribute, 
        (CFTypeRef *)&menuBar
    );

    CFRelease(app);

    if (axErr != kAXErrorSuccess || !menuBar) {
        printf("[!] Could not get menu bar. Error: %d\n", axErr);
        return;
    }

    printf("\n=======================================\n");
    printf(" Menus for frontmost application\n");
    printf("=======================================\n");
    printMenuHierarchy(menuBar, 0);
    CFRelease(menuBar);
}

static void observerCallback(AXObserverRef observer,
                           AXUIElementRef element,
                           CFStringRef notification,
                           void *refcon) {
    if (!element || !notification) return;
    
    // Log the type of focus change
    char notifName[256];
    if (CFStringGetCString(notification, notifName, sizeof(notifName), kCFStringEncodingUTF8)) {
        printf("\nFocus change detected: %s\n", notifName);
    }
    
    listMenusOfFrontmostApp();
}

int main(int argc, const char * argv[]) {
    // Set up signal handling for clean exit
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    printf("Checking accessibility permissions...\n");
    // Check accessibility permissions
    if (!checkAccessibilityPermissions()) {
        printf("[!] Accessibility permissions not granted.\n");
        printf("Please grant permissions in System Settings > Privacy & Security > Accessibility\n");
        printf("Then run the program again.\n");
        return 1;
    }

    printf("Creating accessibility objects...\n");
    AXUIElementRef systemWide = AXUIElementCreateSystemWide();
    if (!systemWide) {
        printf("[!] Failed to create system-wide accessibility object.\n");
        return 1;
    }

    // Use current process ID instead of 0
    pid_t pid = getpid();
    AXObserverRef observer = NULL;
    AXError err = AXObserverCreate(pid, observerCallback, &observer);
    if (err != kAXErrorSuccess) {
        printf("[!] Unable to create AXObserver. Error code: %d\n", err);
        CFRelease(systemWide);
        return 1;
    }

    // Add a small delay to allow the observer to initialize
    usleep(100000); // 100ms delay

    // Add notifications
    err = AXObserverAddNotification(observer, systemWide, kAXFocusedApplicationChangedNotification, NULL);
    if (err != kAXErrorSuccess) {
        printf("[!] Could not add application focus notification. Error: %d\n", err);
        printf("Please make sure accessibility permissions are granted.\n");
    }

    err = AXObserverAddNotification(observer, systemWide, kAXFocusedWindowChangedNotification, NULL);
    if (err != kAXErrorSuccess) {
        printf("[!] Could not add window focus notification. Error: %d\n", err);
    }

    // Store run loop reference for clean shutdown
    gMainRunLoop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(
        gMainRunLoop,
        AXObserverGetRunLoopSource(observer),
        kCFRunLoopDefaultMode
    );

    printf("\nInitializing menu listing...\n");
    // Initial menu listing
    listMenusOfFrontmostApp();

    printf("\nMonitoring focus changes. Press Ctrl+C to exit.\n");
    CFRunLoopRun();

    // Cleanup
    if (observer) {
        AXObserverRemoveNotification(observer, systemWide, kAXFocusedApplicationChangedNotification);
        AXObserverRemoveNotification(observer, systemWide, kAXFocusedWindowChangedNotification);
        CFRelease(observer);
    }
    if (systemWide) CFRelease(systemWide);

    printf("Shutdown complete.\n");
    return 0;
}
