#define UI_MODULE_VERSION "0.0.1"

#include <node.h>
#include <uv.h>
#include <iostream>
#include "ui-module.h"
#include "../ui-window/ui-window.h"
#include "../perf-trace/perf-trace.h"

using namespace v8;

void UiInit(Handle<Object> exports
#ifndef BUILDING_NODE_EXTENSION
    ,Handle<Value> unused, Handle<Context> context, void* priv
#endif
        ) {
    UiWindow::Init(exports);
    UiModule::Init(exports);
}

#ifdef BUILDING_NODE_EXTENSION
NODE_MODULE(ui_wnd, UiInit)
#else
NODE_MODULE_CONTEXT_AWARE_BUILTIN(ui_wnd, UiInit)
#endif

v8::Persistent<v8::Object> UiModule::EnginePropsPersistent;
uv_mutex_t UiModule::_initMutex;
uv_thread_t UiModule::_nodeThread;
UI_RESULT UiModule::_initCode;
char* UiModule::_engineName;
char* UiModule::_engineVersion;

struct Args {
    int argc;
    char** argv;
    Args(int argc, char** argv) {
        this->argc = argc;
        this->argv = argv;
    }
};

int UiModule::Main(int argc, char* argv[]) {
    PerfTrace::Reg(UI_PERF_EVENT_START);
    uv_mutex_init(&_initMutex);
    uv_mutex_lock(&_initMutex);
    PerfTrace::Reg(UI_PERF_EVENT_MAIN_ARGS);
    Args* args = new Args(argc, argv);
    uv_thread_create(&_nodeThread, StartNode, args);
    _initCode = OsInitialize();
    if (UI_SUCCEEDED(_initCode)) {
        _initCode = UiWindow::Initialize();
    }
    else {
        return -1;
    }
    PerfTrace::Reg(UI_PERF_EVENT_INIT_ENVIRONMENT);
    uv_mutex_unlock(&_initMutex);
    return UiWindow::Main(argc, argv);
}

void UiModule::StartNode(void* arg) {
    PerfTrace::Reg(UI_PERF_EVENT_CREATE_NODE_THREAD);
    Args* args = (Args*)arg;
    int exitCode = node::Start(args->argc, args->argv);
    std::exit(exitCode);
}

char* UiModule::GetEngineVersion() {
    return _engineVersion;
}

bool UiModule::IsCef() {
    return strcmp(_engineName, "cef") == 0;
}

void UiModule::Init(Handle<Object> exports) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    NODE_SET_METHOD(exports, "updateEngineVersion", UiModule::UpdateEngineVersion);
    NODE_SET_METHOD(exports, "getPerfStat", UiModule::GetPerfStat);
    NODE_SET_METHOD(exports, "alert", UiModule::Alert);
    exports->Set(String::NewFromUtf8(isolate, "ALERT_ERROR"), Int32::New(isolate, ALERT_TYPE::ALERT_ERROR));
    exports->Set(String::NewFromUtf8(isolate, "ALERT_INFO"), Int32::New(isolate, ALERT_TYPE::ALERT_INFO));
    exports->Set(String::NewFromUtf8(isolate, "version"), String::NewFromUtf8(isolate, UI_MODULE_VERSION));
    uv_mutex_lock(&_initMutex);
    if (UI_FAILED(_initCode)) {
        uv_mutex_unlock(&_initMutex);
        isolate->ThrowException(Exception::Error(String::Concat(String::NewFromUtf8(isolate, "UI start error: "), Integer::New(isolate, _initCode)->ToString())));
        return;
    }
    Local<Object> engineProps = Object::New(isolate);
    engineProps->Set(String::NewFromUtf8(isolate, "name"), String::NewFromUtf8(isolate, _engineName));
    engineProps->Set(String::NewFromUtf8(isolate, "version"), String::NewFromUtf8(isolate, _engineVersion));
    exports->Set(String::NewFromUtf8(isolate, "engine"), engineProps);
    EnginePropsPersistent.Reset(isolate, engineProps);
    uv_mutex_unlock(&_initMutex);
}

void UiModule::UpdateEngineVersion(const v8::FunctionCallbackInfo<v8::Value>& args) {
    auto res = OsSetEngineVersion();
    if (UI_SUCCEEDED(res)) {
        auto isolate = Isolate::GetCurrent();
        auto engineProps = Local<Object>::New(isolate, EnginePropsPersistent);
        engineProps->Set(String::NewFromUtf8(isolate, "name"), String::NewFromUtf8(isolate, _engineName));
        engineProps->Set(String::NewFromUtf8(isolate, "version"), String::NewFromUtf8(isolate, _engineVersion));
    }
}

void UiModule::GetPerfStat(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    int max = UI_PERF_EVENT::UI_PERF_EVENT_LOAD_DOCUMENT_COMPLETE;
    Local<Array> arr = Array::New(isolate, max + 1);
    for (int i = 0; i <= max; i++) {
        int time = (int)(PerfTrace::TimeStamps[i] + .5);
        arr->Set(i, Integer::New(isolate, time));
    }
    args.GetReturnValue().Set(arr);
}

void UiModule::Alert(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    if (args.Length() == 0) {
        return;
    }
    Utf8String* msg = new Utf8String(args[0]);
    ALERT_TYPE type = ALERT_TYPE::ALERT_INFO;
    if (args.Length() > 1) {
        type = (ALERT_TYPE)args[1]->Int32Value();
    }
    int ret = UiWindow::Alert(msg, type);
    args.GetReturnValue().Set(ret);
}
