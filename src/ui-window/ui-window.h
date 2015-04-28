#pragma once

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <uv.h>
#include <vector>
#include "../utf8-string.h"
#include "../window-config.h"
#include "../const.h"

enum WINDOW_EVENT : int {
    WINDOW_EVENT_READY = 1,
    WINDOW_EVENT_CLOSING = 2,
    WINDOW_EVENT_CLOSED = 3,
    WINDOW_EVENT_RESIZE = 4,
    WINDOW_EVENT_STATE = 5,
    WINDOW_EVENT_MOVE = 6,
    WINDOW_EVENT_DOCUMENT_COMPLETE = 7,
    WINDOW_EVENT_MENU = 8,
    WINDOW_EVENT_MESSAGE = 9,
    WINDOW_EVENT_MESSAGE_CALLBACK = 10,
    WINDOW_EVENT_SELECT_FILE = 11,
};

struct WindowRect {
    int Left;
    int Top;
    int Width;
    int Height;

    WindowRect() {}

    WindowRect(int left, int top, int width, int height) :
        Left(left), Top(top), Width(width), Height(height) {}
};

class UiWindow;

struct WindowEventData {
    WINDOW_EVENT Evt;
    UiWindow* Sender;
    long Arg1;
    long Arg2;
    long Arg3;

    WindowEventData(WINDOW_EVENT evt, long arg1 = 0, long arg2 = 0, long arg3 = 0) : Evt(evt), Arg1(arg1), Arg2(arg2), Arg3(arg3) {}
};

struct WindowOpenFileParams {
    bool Open = true;
    Utf8String* Title = NULL;
    bool Dirs = false;
    Utf8String** Ext = NULL;
    bool Multiple = false;
    v8::Persistent<v8::Function>* Complete;
    
    ~WindowOpenFileParams() {
        if (Title)
            delete Title;
        if (Complete) {
            Complete->Reset();
            delete Complete;
        }
    }
};

class UiWindow : public node::ObjectWrap {
public:
    static void Init(v8::Handle<v8::Object> exports);
    static UI_RESULT Initialize();
    static int Main(int argc, char* argv[]);
    virtual ~UiWindow();
    static int Alert(Utf8String* msg, ALERT_TYPE type);
    bool ShouldClose();

protected:
    UiWindow();

    static UI_RESULT OsInitialize();
    static UiWindow* CreateUiWindow();

    void EmitEvent(WindowEventData* eventData);

    virtual void Show(WindowRect& rect) = 0;
    virtual void Close() = 0;
    virtual void Navigate(Utf8String* url) = 0;
    virtual void PostMsg(Utf8String* msg, v8::Persistent<v8::Function>* callback) = 0;
    virtual void SelectFile(WindowOpenFileParams* param) = 0;
    virtual void MsgCallback(void* callback, Utf8String* result, Utf8String* error) = 0;
    virtual void SetWindowRect(WindowRect& rect) = 0;
    virtual WindowRect GetWindowRect() = 0;
    virtual WindowRect GetScreenRect() = 0;
    virtual Utf8String* GetTitle() = 0;
    virtual WINDOW_STATE GetState() = 0;
    virtual void SetState(WINDOW_STATE state) = 0;
    virtual bool GetResizable() = 0;
    virtual void SetResizable(bool resizable) = 0;
    virtual bool GetFrame() = 0;
    virtual void SetFrame(bool frame) = 0;
    virtual bool GetTopmost() = 0;
    virtual void SetTopmost(bool topmost) = 0;
    virtual double GetOpacity() = 0;
    virtual void SetOpacity(double opacity) = 0;

    WindowConfig* _config;
    UiWindow* _parent;
    uv_thread_t _threadId;
    v8::Persistent<v8::Function> _emitFn;
    v8::Persistent<v8::Function> _onMessageFn;
    const bool _isMainWindow;

private:

    static v8::Persistent<v8::Function> constructor;

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void Show(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void Move(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void Navigate(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void PostMsg(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void SelectFile(const v8::FunctionCallbackInfo<v8::Value>& args);

    static void GetOnMessage(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetOnMessage(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetWidth(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetWidth(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetHeight(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetHeight(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetLeft(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetLeft(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetTop(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetTop(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetTitle(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void GetState(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetState(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetResizable(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetResizable(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetFrame(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetFrame(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetTopmost(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetTopmost(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);
    static void GetOpacity(v8::Local<v8::String> property, const v8::PropertyCallbackInfo<v8::Value>& info);
    static void SetOpacity(v8::Local<v8::String> property, v8::Local<v8::Value> value, const v8::PropertyCallbackInfo<void>& info);

    static void AsyncCallback(uv_async_t *handle);
    void InvokeEventCallback(v8::Isolate* isolate, WindowEventData* data);

    static v8::Handle<v8::Value> JsonStringify(v8::Handle<v8::Value> val);
    static v8::Handle<v8::Value> JsonParse(v8::Handle<v8::Value> val);

    static uv_async_t _uvAsyncHandle;
    static uv_mutex_t _pendingEventsLock;
    static std::vector<WindowEventData*> _pendingEvents;

    static bool _isFirstWindowCreated;
    bool _shouldClose = false;
};
