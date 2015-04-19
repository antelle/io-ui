#pragma once

#include <node.h>

enum UI_PERF_EVENT : int {
    UI_PERF_EVENT_START = 0,
    UI_PERF_EVENT_MAIN_ARGS = 1,
    UI_PERF_EVENT_CREATE_NODE_THREAD = 2,
    UI_PERF_EVENT_INIT_ENVIRONMENT = 3,
    UI_PERF_EVENT_INIT_V8 = 4,
    UI_PERF_EVENT_EXEC_NODEJS = 5,
    UI_PERF_EVENT_DETECT_MAIN = 6,
    UI_PERF_EVENT_INIT_VFS = 7,
    UI_PERF_EVENT_READ_ZIP = 8,
    UI_PERF_EVENT_MAINJS = 9,
    UI_PERF_EVENT_LOAD_UI_MOD = 10,
    UI_PERF_EVENT_LOAD_UI_BINDING = 11,
    UI_PERF_EVENT_START_SERVER = 12,
    UI_PERF_EVENT_CREATE_WINDOW = 13,
    UI_PERF_EVENT_INIT_BROWSER_CONTROL = 14,
    UI_PERF_EVENT_WINDOW_READY = 15,
    UI_PERF_EVENT_CALL_NAVIGATE = 16,
    UI_PERF_EVENT_1ST_REQUEST_SENT = 17,
    UI_PERF_EVENT_1ST_REQUEST_RECEIVED = 18,
    UI_PERF_EVENT_LOAD_INDEX_HTML = 19,
    UI_PERF_EVENT_LOAD_DOCUMENT_COMPLETE = 20,
};

class PerfTrace {
public:
    static double TimeStamps[32];
    static void Reg(UI_PERF_EVENT evt);
    static void RegV8(const v8::FunctionCallbackInfo<v8::Value>& args);
private:
    PerfTrace();
    PerfTrace(PerfTrace& pt);
    static double Freq();
    static double TimeStamp();
    static double _timeFreq;
    static double _first;
};
