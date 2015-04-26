#include <iostream>
#include "ui-window.h"
#include "../perf-trace/perf-trace.h"

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <JavaScriptCore/JavaScriptCore.h>

class UiWindowMac : public UiWindow {
protected:
    virtual void Show(WindowRect& rect);
    virtual void Close();
    virtual void Navigate(Utf8String* url);virtual void PostMsg(Utf8String* msg, v8::Persistent<v8::Function>* callback);
    virtual void MsgCallback(void* callback, Utf8String* result, Utf8String* error);
    
    virtual void SetWindowRect(WindowRect& rect);
    virtual WindowRect GetWindowRect();
    virtual WindowRect GetScreenRect();
    virtual Utf8String* GetTitle();
    virtual WINDOW_STATE GetState();
    virtual void SetState(WINDOW_STATE state);
    virtual bool GetResizable();
    virtual void SetResizable(bool resizable);
    virtual bool GetFrame();
    virtual void SetFrame(bool frame);
    virtual bool GetTopmost();
    virtual void SetTopmost(bool frame);
    virtual double GetOpacity();
    virtual void SetOpacity(double opacity);
public:
    UiWindowMac();
    void Emit(WindowEventData* data);
    void WindowClosed();
    static bool _mainWindowShown;
    NSWindow* _window;
    WebView* _webView;
    void CreateWindowMenu();
private:
    WindowRect _rect;
    void AddMenu(UiMenu* menu, NSMenu* parentMenu);
    static void AddStandardEditMenu(NSMenu* editMenu);
};

@interface IoUiWindow : NSWindow
@end
@implementation IoUiWindow
-(BOOL)canBecomeKeyWindow {
    return YES;
}
-(BOOL)canBecomeMainWindow {
    return YES;
}
@end

@interface IoUiMenuItem : NSMenuItem {
    UiMenu* uiMenu;
}
@property UiMenu* uiMenu;
@end
@implementation IoUiMenuItem
@synthesize uiMenu;
@end

@interface IoUiWindowDelegate : NSObject <NSWindowDelegate> {
    UiWindowMac* _window;
}
- (id) initWithWindow:(UiWindowMac*)window;
@end

@implementation IoUiWindowDelegate

- (id) initWithWindow:(UiWindowMac*)window {
    self = [super init];
    if (self) {
        _window = window;
    }
    return self;
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
    _window->Emit(new WindowEventData(WINDOW_EVENT_STATE, WINDOW_STATE_MINIMIZED));
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
    _window->Emit(new WindowEventData(WINDOW_EVENT_STATE, WINDOW_STATE_NORMAL));
}

- (bool)windowShouldZoom:(NSWindow*)window {
    _window->Emit(new WindowEventData(WINDOW_EVENT_STATE, WINDOW_STATE_MAXIMIZED));
    return YES;
}

- (NSRect)windowWillUseStandardFrame:(NSWindow*)window
                        defaultFrame:(NSRect)frame {    return frame;
}

- (void)windowDidResize:(NSNotification*)notification {
    NSRect rect = _window->_window.frame;
    _window->Emit(new WindowEventData(WINDOW_EVENT_RESIZE, rect.size.width, rect.size.height));
}

- (void)windowDidMove:(NSNotification*)notification {
    NSRect rect = _window->_window.frame;
    NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
    rect.origin.y = screenRect.size.height - rect.size.height - rect.origin.y;
    _window->Emit(new WindowEventData(WINDOW_EVENT_MOVE, rect.origin.x, rect.origin.y));
}

- (BOOL)windowShouldClose:(id)window {
    return YES;
}

- (void)windowWillClose:(id)window {
    _window->WindowClosed();
}

- (void)menuItemHit:(IoUiMenuItem*)sender {
    UiMenu* uiMenu = sender.uiMenu;
    _window->Emit(new WindowEventData(WINDOW_EVENT_MENU, (long)uiMenu));
}

@end

@interface IoUiWebExternal: NSObject {
    UiWindowMac* _window;
}
- (id) initWithWindow:(UiWindowMac*)window;
- (void) pm:(NSString*)msg withCallback:(WebScriptObject*)callback;
@end

@implementation IoUiWebExternal
- (id) initWithWindow:(UiWindowMac*)window {
    self = [super init];
    if (self) {
        _window = window;
    }
    return self;
}

- (void) pm:(NSString*)msg withCallback:(WebScriptObject*)callback {
    JSObjectRef ref = NULL;
    if (callback) {
        JSContextRef ctx = _window->_webView.mainFrame.globalContext;
        ref = [callback JSObject];
        if (JSObjectIsFunction(ctx, ref)) {
            JSValueProtect(ctx, ref);
        } else {
            ref = NULL;
        }
    }
    Utf8String* msgStr = new Utf8String((char*)[msg UTF8String]);
    _window->Emit(new WindowEventData(WINDOW_EVENT_MESSAGE, (long)msgStr, (long)ref));
}

+ (NSString *) webScriptNameForSelector:(SEL)sel {
    if (sel == @selector(pm:withCallback:))
        return @"pm";
    return nil;
}

+ (BOOL) isSelectorExcludedFromWebScript:(SEL)sel {
    return NO;
}
@end

@interface IoUiWebView : WebView
@end

@implementation IoUiWebView
- (void)keyDown:(NSEvent *)theEvent {
}

- (BOOL)performKeyEquivalent:(NSEvent *)theEvent {
    NSString * chars = [theEvent characters];
    BOOL status = NO;

    if ([theEvent modifierFlags] & NSCommandKeyMask){

        if ([chars isEqualTo:@"a"]){
            [self selectAll:nil];
            status = YES;
        }

        if ([chars isEqualTo:@"c"]){
            [self copy:nil];
            status = YES;
        }

        if ([chars isEqualTo:@"v"]){
            [self paste:nil];
            status = YES;
        }

        if ([chars isEqualTo:@"x"]){
            [self cut:nil];
            status = YES;
        }
    }
    return status;
}

@end

@interface IoUiWebViewDelegate : NSObject {
    UiWindowMac* _window;
}
- (id) initWithWindow:(UiWindowMac*)window;
@end

@implementation IoUiWebViewDelegate

- (id) initWithWindow:(UiWindowMac*)window {
    self = [super init];
    if (self) {
        _window = window;
    }
    return self;
}

- (void)webView:(WebView *)sender didFinishLoadForFrame:(WebFrame *)frame {
    _window->Emit(new WindowEventData(WINDOW_EVENT_DOCUMENT_COMPLETE));
}

- (void)webView:(WebView *)sender didCommitLoadForFrame:(WebFrame *)frame {
    PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_LOAD_INDEX_HTML);
    [sender stringByEvaluatingJavaScriptFromString:@"window.backend = {"
        "postMessage: function(data, cb) { external.pm(JSON.stringify(data), cb ? function(res, err) { if (typeof cb === 'function') cb(JSON.parse(res), err); } : null); },"
        "onMessage: null"
        "};"];
    IoUiWebExternal* webExternal = [[IoUiWebExternal alloc] initWithWindow:_window];
    id win = [sender windowScriptObject];
    [win setValue:webExternal forKey:@"external"];
}

- (void)webView:(WebView *)sender didStartProvisionalLoadForFrame:(WebFrame *)frame {
    PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_1ST_REQUEST_SENT);
}

- (void)webView:(WebView *)sender didReceiveTitle:(NSString *)title forFrame:(WebFrame *)frame {
    _window->_window.title = title;
}

- (NSArray *)webView:(WebView *)sender
contextMenuItemsForElement:(NSDictionary *)element
    defaultMenuItems:(NSArray *)defaultMenuItems {
    return nil;
}

- (void)webView:(WebView *)sender runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSInformationalAlertStyle;
    [alert addButtonWithTitle:@"OK"];
    [alert setMessageText:message];
    [alert runModal];
}

- (BOOL)webView:(WebView *)sender runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WebFrame *)frame {
    NSAlert *alert = [[NSAlert alloc] init];
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setMessageText:message];
    auto result = [alert runModal];
    return result == NSAlertFirstButtonReturn;
}

@end

bool UiWindowMac::_mainWindowShown = false;

int UiWindow::Main(int argc, char* argv[]) {
    NSRunLoop *threadRunLoop = [NSRunLoop currentRunLoop];
    while ([threadRunLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:1]]) {
        if (UiWindowMac::_mainWindowShown) {
            break;
        }
    }
    [[NSApplication sharedApplication] activateIgnoringOtherApps:NO];
    [[NSApplication sharedApplication] run];
    return 0;
}

UiWindowMac::UiWindowMac() {
}

int UiWindow::Alert(Utf8String* msg, ALERT_TYPE type) {
    __block int result;
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSAlert *alert = [[NSAlert alloc] init];
        alert.alertStyle = type == ALERT_TYPE::ALERT_ERROR ? NSCriticalAlertStyle : NSInformationalAlertStyle;
        [alert addButtonWithTitle:@"OK"];
        [alert setMessageText:[NSString stringWithUTF8String:(char*)*msg]];
        result = [alert runModal] == NSAlertFirstButtonReturn ? 0 : 1;
        delete msg;
    });
    return result;
}

UiWindow* UiWindow::CreateUiWindow() {
    return new UiWindowMac();
}

void UiWindowMac::Emit(WindowEventData* data) {
    EmitEvent(data);
}

void UiWindowMac::WindowClosed() {
    EmitEvent(new WindowEventData(WINDOW_EVENT_CLOSED));
    if (_isMainWindow)
        [NSApp performSelector:@selector(terminate:) withObject:nil afterDelay:0.0];
}

void UiWindowMac::Show(WindowRect& rect) {
    _rect = rect;
    dispatch_async(dispatch_get_main_queue(), ^{
        IoUiWindowDelegate* windowDelegate = [[IoUiWindowDelegate alloc] initWithWindow:this];
        IoUiWebViewDelegate* webViewDelegate = [[IoUiWebViewDelegate alloc] initWithWindow:this];

        NSUInteger styles =
            (_config->Frame ? NSTitledWindowMask : 0)
            | NSClosableWindowMask
            | NSMiniaturizableWindowMask
            | (_config->Resizable ? NSResizableWindowMask : 0);
        
        NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
        _rect.Top = screenRect.size.height - (CGFloat)_rect.Height - (CGFloat)_rect.Top;
        NSRect windowRect = { {(CGFloat)_rect.Left, (CGFloat)_rect.Top} , {(CGFloat)_rect.Width, (CGFloat)_rect.Height} };
        _window = [[IoUiWindow alloc]
                           initWithContentRect:windowRect
                           styleMask:styles
                           backing:NSBackingStoreBuffered
                           defer:NO];
        [_window setDelegate:windowDelegate];
        NSString* title = [NSString stringWithUTF8String:(char*)*_config->Title];
        [_window setTitle:title];
        [_window setReleasedWhenClosed:YES];
        [_window setOpaque:YES];
        [_window setBackgroundColor:[NSColor clearColor]];
        if (_config->Opacity < 1) {
            [_window setOpaque:NO];
            [_window setAlphaValue:_config->Opacity];
        }
        CreateWindowMenu();
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_CREATE_WINDOW);

        NSView* contentView = [_window contentView];
        _webView = [[IoUiWebView alloc] initWithFrame:contentView.frame];
        [_webView setFrameLoadDelegate:webViewDelegate];
        [_webView setUIDelegate:webViewDelegate];
        [contentView addSubview:_webView];
        contentView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_INIT_BROWSER_CONTROL);

        if (_config->State != WINDOW_STATE_HIDDEN)
            [_window makeKeyAndOrderFront:nil];
        if (_config->Topmost)
            [_window setLevel:NSScreenSaverWindowLevel];
        if (_config->State == WINDOW_STATE_MAXIMIZED)
            [_window zoom:nil];
        else if (_config->State == WINDOW_STATE_MINIMIZED)
            [_window miniaturize:nil];
        else if (_config->State == WINDOW_STATE_HIDDEN)
            [_window orderOut:nil];
        else if (_config->State == WINDOW_STATE_FULLSCREEN) {
            NSDictionary* options = @{
                                      NSFullScreenModeApplicationPresentationOptions:
                                          @(NSApplicationPresentationHideMenuBar | NSApplicationPresentationHideDock)
                                      };
            [_webView enterFullScreenMode:[NSScreen mainScreen] withOptions:options];
        }

        UiWindowMac::_mainWindowShown = true;
        EmitEvent(new WindowEventData(WINDOW_EVENT_READY));
    });
}

UI_RESULT UiWindow::OsInitialize() {
    return UI_S_OK;
}

void UiWindowMac::Close() {
    dispatch_sync(dispatch_get_main_queue(), ^{
        [_window orderOut: nil];
        [_window close];
    });
}

void UiWindowMac::Navigate(Utf8String* url) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSString* urlString = [NSString stringWithUTF8String:(char*)*url];
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_CALL_NAVIGATE);
        [[_webView mainFrame] loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:urlString]]];
    });
    delete url;
}

void UiWindowMac::PostMsg(Utf8String* msg, v8::Persistent<v8::Function>* callback) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString* script = [NSString
            stringWithFormat:@"JSON.stringify(typeof backend.onMessage === 'function' ? backend.onMessage(%@):null)",
            [NSString stringWithUTF8String:*msg]];
        delete msg;
        NSString* result = [_webView stringByEvaluatingJavaScriptFromString:script];
        Utf8String* resultStr = new Utf8String((char*)[result UTF8String]);
        EmitEvent(new WindowEventData(WINDOW_EVENT_MESSAGE_CALLBACK, (long)callback, (long)resultStr, (long)NULL));
    });
}

void UiWindowMac::MsgCallback(void* callback, Utf8String* result, Utf8String* error) {
    dispatch_async(dispatch_get_main_queue(), ^{
        if (callback) {
            JSObjectRef ref = (JSObjectRef)callback;
            JSContextRef ctx = [[_webView mainFrame] globalContext];
            JSStringRef resJsStr = JSStringCreateWithUTF8CString(*result);
            JSValueRef args[] = { JSValueMakeString(ctx, resJsStr) };
            JSObjectCallAsFunction(ctx, ref, NULL, 1, args, NULL);
            JSStringRelease(resJsStr);
            JSValueUnprotect(ctx, ref);
        }
        if (result)
            delete result;
        if (error)
            delete error;
    });
}

void UiWindowMac::CreateWindowMenu() {
    // https://developer.apple.com/library/mac/documentation/UserExperience/Conceptual/OSXHIGuidelines/MenuBarMenus.html
    if (_isMainWindow) {
        id appName = [[NSProcessInfo processInfo] processName];

        id appMenu = [[NSMenu alloc] init];
        [appMenu addItem:[[IoUiMenuItem alloc] initWithTitle:[@"About " stringByAppendingString:appName]
            action:@selector(terminate:) keyEquivalent:@""]];
        [appMenu addItem:[NSMenuItem separatorItem]];
        [appMenu addItem:[[IoUiMenuItem alloc] initWithTitle:[@"Hide " stringByAppendingString:appName]
            action:@selector(hide:) keyEquivalent:@"h"]];
        id item = [[IoUiMenuItem alloc] initWithTitle:@"Hide Others"
            action:@selector(hideOtherApplications:) keyEquivalent:@"h"];
        [item setKeyEquivalentModifierMask:NSCommandKeyMask | NSAlternateKeyMask];
        [appMenu addItem:item];
        [appMenu addItem:[[IoUiMenuItem alloc] initWithTitle:@"Show All"
            action:@selector(unhideAllApplications:) keyEquivalent:@""]];
        [appMenu addItem:[NSMenuItem separatorItem]];
        [appMenu addItem:[[IoUiMenuItem alloc] initWithTitle:[@"Quit " stringByAppendingString:appName]
            action:@selector(terminate:) keyEquivalent:@"q"]];
        
        NSMenu* menubar = [[NSMenu alloc] init];
        id appMenuItem = [[NSMenuItem alloc] init];
        [appMenuItem setSubmenu:appMenu];
        [menubar addItem:appMenuItem];
        
        if (_config->Menu) {
            AddMenu(_config->Menu, menubar);
        }

        [NSApp setMainMenu:menubar];
    }
}

void UiWindowMac::AddMenu(UiMenu* menu, NSMenu* parentMenu) {
    if (menu->Type == MENU_TYPE::MENU_TYPE_SEPARATOR) {
        [parentMenu addItem:[NSMenuItem separatorItem]];
    } else {
        NSString* title = [NSString stringWithUTF8String:*menu->Title];
        NSRange range = [title rangeOfString:@"&"];
        NSString* keyEquivalent = @"";
        if (range.location != NSNotFound && range.location < title.length - 1) {
            range.location++;
            keyEquivalent = [title substringWithRange:range];
            title = [title stringByReplacingOccurrencesOfString:@"&" withString:@""];
        }
        bool isEdit = [title isEqualToString:@"Edit"];
        if ((menu->Type == MENU_TYPE::MENU_TYPE_SIMPLE && menu->Items) || isEdit) {
            NSMenu* subMenu = [[NSMenu alloc] initWithTitle:title];
            NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                action:nil keyEquivalent:keyEquivalent];
            if (isEdit) {
                AddStandardEditMenu(subMenu);
            }
            if (menu->Items) {
                if (isEdit) {
                    [subMenu addItem:[NSMenuItem separatorItem]];
                }
                AddMenu(menu->Items, subMenu);
            }
            [item setSubmenu:subMenu];
            [parentMenu addItem:item];
        }
        else {
            NSMenuItem* existingMenuItem = [parentMenu itemWithTitle:title];
            if (!existingMenuItem) {
                IoUiMenuItem* item = [[IoUiMenuItem alloc] initWithTitle:title
                    action:@selector(menuItemHit:) keyEquivalent:keyEquivalent];
                [item setTarget:_window.delegate];
                item.uiMenu = menu;
                [parentMenu addItem:item];
            }
        }
    }
    if (menu->Next) {
        AddMenu(menu->Next, parentMenu);
    }
}

void UiWindowMac::AddStandardEditMenu(NSMenu* editMenu) {
    [editMenu addItem:[[IoUiMenuItem alloc] initWithTitle:@"Undo"
                                                   action:@selector(undo) keyEquivalent:@"z"]];
    auto item = [[IoUiMenuItem alloc] initWithTitle:@"Redo"
                                        action:@selector(redo) keyEquivalent:@"z"];
    [item setKeyEquivalentModifierMask:NSCommandKeyMask | NSShiftKeyMask];
    [editMenu addItem:item];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItem:[[IoUiMenuItem alloc] initWithTitle:@"Cut"
                                                   action:@selector(cut:) keyEquivalent:@"x"]];
    [editMenu addItem:[[IoUiMenuItem alloc] initWithTitle:@"Copy"
                                                   action:@selector(copy:) keyEquivalent:@"c"]];
    [editMenu addItem:[[IoUiMenuItem alloc] initWithTitle:@"Paste"
                                                   action:@selector(paste:) keyEquivalent:@"v"]];
    [editMenu addItem:[[IoUiMenuItem alloc] initWithTitle:@"Select All"
                                                   action:@selector(selectAll:) keyEquivalent:@"a"]];
}

WindowRect UiWindowMac::GetWindowRect() {
    __block NSRect rect;
    dispatch_sync(dispatch_get_main_queue(), ^{
        rect = _window.frame;
        NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
        rect.origin.y = screenRect.size.height - rect.size.height - rect.origin.y;
    });
    return WindowRect(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
}

WindowRect UiWindowMac::GetScreenRect() {
    __block NSRect rect;
    dispatch_sync(dispatch_get_main_queue(), ^{
        rect = [[NSScreen mainScreen] visibleFrame];
    });
    return WindowRect(0, 0, rect.size.width, rect.size.height);
}

void UiWindowMac::SetWindowRect(WindowRect& rect) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSRect screenRect = [[NSScreen mainScreen] visibleFrame];
        CGFloat top = screenRect.size.height - (CGFloat) rect.Height - (CGFloat)rect.Top;
        NSRect windowRect = { {(CGFloat)rect.Left, top} , {(CGFloat)rect.Width, (CGFloat)rect.Height} };
        [_window setFrame:windowRect display:YES];
    });
}

Utf8String* UiWindowMac::GetTitle() {
    return new Utf8String((char*)[[_window title] UTF8String]);
}

WINDOW_STATE UiWindowMac::GetState() {
    __block WINDOW_STATE state;
    dispatch_sync(dispatch_get_main_queue(), ^{
        if (_window.isMiniaturized)
            state = WINDOW_STATE::WINDOW_STATE_MINIMIZED;
        else if (_window.isZoomed)
            state = WINDOW_STATE::WINDOW_STATE_MAXIMIZED;
        else if (!_window.isVisible)
            state = WINDOW_STATE::WINDOW_STATE_HIDDEN;
        else if (_webView.isInFullScreenMode)
            state = WINDOW_STATE::WINDOW_STATE_FULLSCREEN;
        else
            state = WINDOW_STATE::WINDOW_STATE_NORMAL;
    });
    return state;
}

void UiWindowMac::SetState(WINDOW_STATE state) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        if (![_window isVisible] && (state != WINDOW_STATE::WINDOW_STATE_HIDDEN))
            [_window makeKeyAndOrderFront:nil];
        if ([_window isMiniaturized] && (state != WINDOW_STATE::WINDOW_STATE_MINIMIZED))
            [_window deminiaturize:nil];
        if ([_window isZoomed] && (state != WINDOW_STATE::WINDOW_STATE_MAXIMIZED))
            _window.isZoomed = FALSE;
        if (_webView.isInFullScreenMode)
            [_webView exitFullScreenModeWithOptions:nil];
        switch (state) {
        case WINDOW_STATE::WINDOW_STATE_NORMAL:
            break;
        case WINDOW_STATE::WINDOW_STATE_HIDDEN:
            if (_window.isVisible)
                [_window orderOut:nil];
            break;
        case WINDOW_STATE::WINDOW_STATE_MAXIMIZED:
            if (!_window.isZoomed)
                [_window zoom:nil];
            break;
        case WINDOW_STATE::WINDOW_STATE_MINIMIZED:
            if (!_window.isMiniaturized)
                [_window miniaturize:nil];
            break;
        case WINDOW_STATE::WINDOW_STATE_FULLSCREEN:
            {
                NSDictionary* options = @{
                                          NSFullScreenModeApplicationPresentationOptions:
                                              @(NSApplicationPresentationHideMenuBar | NSApplicationPresentationHideDock)
                                          };
                [_webView enterFullScreenMode:[NSScreen mainScreen] withOptions:options];
            }
            break;
        }
    });
}

bool UiWindowMac::GetResizable() {
    __block bool resizable;
    dispatch_sync(dispatch_get_main_queue(), ^{
        resizable = [_window styleMask] & NSResizableWindowMask;
    });
    return resizable;
}

void UiWindowMac::SetResizable(bool resizable) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSUInteger styleMask = [_window styleMask];
        if (resizable)
            styleMask |= NSResizableWindowMask;
        else
            styleMask &= ~NSResizableWindowMask;
        [_window setStyleMask:styleMask];
    });
}

bool UiWindowMac::GetFrame() {
    __block bool frame;
    dispatch_sync(dispatch_get_main_queue(), ^{
        frame = [_window styleMask] & NSTitledWindowMask;
    });
    return frame;
}

void UiWindowMac::SetFrame(bool frame) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSUInteger styleMask = [_window styleMask];
        NSUInteger flags = NSTitledWindowMask;
        if (frame)
            styleMask |= flags;
        else
            styleMask &= ~flags;
        [_window setStyleMask:styleMask];
    });
}

bool UiWindowMac::GetTopmost() {
    __block bool topmost;
    dispatch_sync(dispatch_get_main_queue(), ^{
        topmost = [_window level] == NSScreenSaverWindowLevel;
    });
    return topmost;
}

void UiWindowMac::SetTopmost(bool topmost) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        [_window setLevel:topmost ? NSScreenSaverWindowLevel : NSNormalWindowLevel];
    });
}

double UiWindowMac::GetOpacity() {
    __block double opacity;
    dispatch_sync(dispatch_get_main_queue(), ^{
        opacity = [_window alphaValue];
    });
    return opacity;
}

void UiWindowMac::SetOpacity(double opacity) {
    dispatch_sync(dispatch_get_main_queue(), ^{
        [_window setOpaque:(opacity < 1) ? NO : YES];
        [_window setAlphaValue:opacity];
    });
}
