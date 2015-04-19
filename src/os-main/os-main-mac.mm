#include "../ui-module/ui-module.h"
#include <node.h>

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

@interface IoUiApplication : NSApplication {
}
@end
@implementation IoUiApplication
@end

@interface IoUiAppDelegate : NSObject<NSApplicationDelegate> {
}
@end
@implementation IoUiAppDelegate
- (void)applicationWillTerminate:(NSNotification *)aNotification {
}
@end

int main(int argc, char *argv[]) {
    @autoreleasepool {
        NSApplication* app = [IoUiApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        IoUiAppDelegate* appDelegate = [[IoUiAppDelegate alloc] init];
        [app setDelegate:appDelegate];
        
        NSFileManager *fileMgr = [[NSFileManager alloc] init];
        NSString* dir = [[[NSBundle mainBundle] bundlePath] stringByDeletingLastPathComponent];
        [fileMgr changeCurrentDirectoryPath:dir];
        
        for (int i = 0; i < argc; i++) {
            if (!strncmp(argv[i], "-psn", 4)) {
                argv[i][0] = '\0';
            }
        }
        
        return UiModule::Main(argc, argv);
    }
}