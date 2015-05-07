#include <node.h>
#include "ui-module.h"

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

using namespace v8;

UI_RESULT UiModule::OsInitialize() {
    return OsSetEngineVersion();
}

UI_RESULT UiModule::OsSetEngineVersion() {
    _engineName = (char*)"webkit";
    NSString* webkitPath = @"/System/Library/Frameworks/WebKit.framework/Resources/Info.plist";
    NSDictionary* webkitDict = [[NSDictionary alloc] initWithContentsOfFile:webkitPath];
    NSString* ver = [webkitDict valueForKey:@"CFBundleVersion"];
    _engineVersion = new char[ver.length];
    auto verStr = [ver UTF8String];
    auto ix = verStr[0] == '1' ? 2 : 1;
    strcpy(_engineVersion, &verStr[ix]);
    return UI_S_OK;
}

char* OsGetSupportedCefVersion() {
    return NULL;
}
