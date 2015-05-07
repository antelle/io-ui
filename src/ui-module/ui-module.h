#pragma once

#include <node.h>
#include <uv.h>
#include "../const.h"

class UiModule {
public:
    static void Init(v8::Handle<v8::Object> exports);
    static int Main(int argc, char* argv[]);
    static char* GetEngineVersion();
    static bool IsCef();
private:
    static void UpdateEngineVersion(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void GetPerfStat(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void GetSupportedCefVersion(const v8::FunctionCallbackInfo<v8::Value>& args);

    static v8::Persistent<v8::Object> EnginePropsPersistent;

    static UI_RESULT OsInitialize();
    static UI_RESULT OsSetEngineVersion();
    static char* OsGetSupportedCefVersion();
    static void StartNode(void* arg);

    static uv_mutex_t _initMutex;
    static uv_thread_t _nodeThread;
    static UI_RESULT _initCode;
    static char* _engineName;
    static char* _engineVersion;
};
