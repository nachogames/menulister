#include <ApplicationServices/ApplicationServices.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

// Function declarations
static void printMenuHierarchy(AXUIElementRef menuElement, int indent);
static Boolean checkAccessibilityPermissions(void);
static void listMenusForPID(pid_t pid);

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

static void listMenusForPID(pid_t pid) {
    AXUIElementRef app = AXUIElementCreateApplication(pid);
    if (!app) {
        printf("[!] Could not create application reference for PID %d.\n", pid);
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
        printf("[!] Could not get menu bar for PID %d. Error: %d\n", pid, axErr);
        return;
    }

    printf("\n=======================================\n");
    printf(" Menus for application (PID: %d)\n", pid);
    printf("=======================================\n");
    printMenuHierarchy(menuBar, 0);
    CFRelease(menuBar);
}

void print_usage(const char* program_name) {
    printf("Usage: %s <pid>\n", program_name);
    printf("  pid: Process ID of the application to show menus for\n");
    printf("\nExample: %s 1234\n", program_name);
}

int main(int argc, const char * argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);
    if (pid <= 0) {
        printf("Error: Invalid PID specified\n");
        print_usage(argv[0]);
        return 1;
    }

    printf("Checking accessibility permissions...\n");
    if (!checkAccessibilityPermissions()) {
        printf("[!] Accessibility permissions not granted.\n");
        printf("Please grant permissions in System Settings > Privacy & Security > Accessibility\n");
        printf("Then run the program again.\n");
        return 1;
    }

    listMenusForPID(pid);
    return 0;
}
