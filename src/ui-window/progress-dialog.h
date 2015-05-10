#pragma once

#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>
#include <uv.h>
#include "../utf8-string.h"

class ProgressDialog : public node::ObjectWrap {
public:
    static ProgressDialog* CreateProgressDialog();
    virtual ~ProgressDialog();
    virtual void ShowOsWindow() = 0;
    virtual void Close() = 0;
    virtual void SetValue(int value, Utf8String* description) = 0;

    static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
    static ProgressDialog* CreateNew(const v8::FunctionCallbackInfo<v8::Value>& args);
protected:
    ProgressDialog();
    void EmitClosedEvent();

    Utf8String* _title = NULL;
    v8::Persistent<v8::Function> _closed;
    int _value = 0;
    Utf8String* _text = NULL;
private:
    static void Init();
    static v8::Persistent<v8::Function> _constructor;
    static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
    static void SetValue(const v8::FunctionCallbackInfo<v8::Value>& args);
    static uv_async_t _uvAsyncHandle;
    static void AsyncCallback(uv_async_t *handle);
};
