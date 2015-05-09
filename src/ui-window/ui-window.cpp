#include <node.h>
#include <iostream>
#include "ui-window.h"
#include "progress-dialog.h"
#include "../perf-trace/perf-trace.h"

using namespace v8;

Persistent<Function> UiWindow::_constructor;
bool UiWindow::_isFirstWindowCreated = false;
uv_async_t UiWindow::_uvAsyncHandle;
uv_mutex_t UiWindow::_pendingEventsLock;
WindowEventData* UiWindow::_pendingEvents;

UiWindow::UiWindow() : _isMainWindow(!_isFirstWindowCreated) {
    _isFirstWindowCreated = true;
}

UiWindow::~UiWindow() {
    _onMessageFn.Reset();
    delete _config;
}

UI_RESULT UiWindow::Initialize() {
    uv_mutex_init(&_pendingEventsLock);
    return UiWindow::OsInitialize();
}

void UiWindow::Init(Handle<Object> exports) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "UiWindow"));
    tpl->InstanceTemplate()->SetInternalFieldCount(3);

    NODE_SET_METHOD(tpl, "alert", Alert);
    NODE_SET_METHOD(tpl, "showProgressDlg", ShowProgressDlg);

    NODE_SET_PROTOTYPE_METHOD(tpl, "show", Show);
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
    NODE_SET_PROTOTYPE_METHOD(tpl, "move", Move);
    NODE_SET_PROTOTYPE_METHOD(tpl, "navigate", Navigate);
    NODE_SET_PROTOTYPE_METHOD(tpl, "postMessage", PostMsg);
    NODE_SET_PROTOTYPE_METHOD(tpl, "selectFile", SelectFile);

    auto protoTpl = tpl->PrototypeTemplate();

    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "onMessage"), GetOnMessage, SetOnMessage, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "width"), GetWidth, SetWidth, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "height"), GetHeight, SetHeight, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "left"), GetLeft, SetLeft, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "top"), GetTop, SetTop, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "title"), GetTitle, NULL, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "state"), GetState, SetState, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "resizable"), GetResizable, SetResizable, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "frame"), GetFrame, SetFrame, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "topmost"), GetTopmost, SetTopmost, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);
    protoTpl->SetAccessor(String::NewFromUtf8(isolate, "opacity"), GetOpacity, SetOpacity, Handle<Value>(), DEFAULT, PropertyAttribute::DontDelete);

    tpl->Set(isolate, "STATE_NORMAL", Int32::New(isolate, WINDOW_STATE::WINDOW_STATE_NORMAL));
    tpl->Set(isolate, "STATE_HIDDEN", Int32::New(isolate, WINDOW_STATE::WINDOW_STATE_HIDDEN));
    tpl->Set(isolate, "STATE_MAXIMIZED", Int32::New(isolate, WINDOW_STATE::WINDOW_STATE_MAXIMIZED));
    tpl->Set(isolate, "STATE_MINIMIZED", Int32::New(isolate, WINDOW_STATE::WINDOW_STATE_MINIMIZED));
    tpl->Set(isolate, "STATE_FULLSCREEN", Int32::New(isolate, WINDOW_STATE::WINDOW_STATE_FULLSCREEN));

    tpl->Set(isolate, "MENU_TYPE_SIMPLE", Int32::New(isolate, MENU_TYPE::MENU_TYPE_SIMPLE));
    tpl->Set(isolate, "MENU_TYPE_SEPARATOR", Int32::New(isolate, MENU_TYPE::MENU_TYPE_SEPARATOR));

    tpl->Set(String::NewFromUtf8(isolate, "ALERT_ERROR"), Int32::New(isolate, ALERT_TYPE::ALERT_ERROR));
    tpl->Set(String::NewFromUtf8(isolate, "ALERT_INFO"), Int32::New(isolate, ALERT_TYPE::ALERT_INFO));
    tpl->Set(String::NewFromUtf8(isolate, "ALERT_QUESTION"), Int32::New(isolate, ALERT_TYPE::ALERT_QUESTION));

    _constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "Window"), tpl->GetFunction());
}

void UiWindow::New(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.IsConstructCall() || args[0]->IsUndefined()) {
        UiWindow* _this = CreateUiWindow();
        Handle<Object> obj = Handle<Object>::Cast(args[0]);
        _this->_config = new WindowConfig(obj);
        _this->Wrap(args.This());

        Local<Value> emit = _this->handle()->Get(String::NewFromUtf8(isolate, "emit"));
        Local<Function> emitFn = Local<Function>::Cast(emit);
        _this->_emitFn.Reset(isolate, emitFn);

        args.GetReturnValue().Set(args.This());
    }
}

void UiWindow::Show(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    UiWindow* _this = ObjectWrap::Unwrap<UiWindow>(args.This());
    if (_this->_isMainWindow)
        uv_async_init(uv_default_loop(), &_this->_uvAsyncHandle, &UiWindow::AsyncCallback);

    WindowRect rect;
    WindowRect screen = _this->GetScreenRect();

    auto config = _this->_config;
    if (config->Width.Position == WINDOW_POSITION_PX)
        rect.Width = config->Width;
    else if (config->Width.Position == WINDOW_POSITION_PERCENT)
        rect.Width = (int)((double)screen.Width / 100. * config->Width);
    else
        rect.Width = screen.Width / 2;
    if (config->Height.Position == WINDOW_POSITION_PX)
        rect.Height = config->Height;
    else if (config->Height.Position == WINDOW_POSITION_PERCENT)
        rect.Height = (int)((double)screen.Height / 100. * config->Height);
    else
        rect.Height = screen.Height / 2;
    if (config->Left.Position == WINDOW_POSITION_PX)
        rect.Left = config->Left;
    else if (config->Left.Position == WINDOW_POSITION_PERCENT)
        rect.Left = (int)((double)screen.Width / 100. * config->Left);
    else
        rect.Left = (int)((double)screen.Width / 2. - rect.Width / 2.);
    if (config->Top.Position == WINDOW_POSITION_PX)
        rect.Top = config->Top;
    else if (config->Top.Position == WINDOW_POSITION_PERCENT)
        rect.Top = (int)((double)screen.Height / 100. * config->Top);
    else
        rect.Top = (int)((double)screen.Height / 2. - rect.Height / 2.);

    UiWindow* parent = NULL;
    if (args.Length() >= 1 && args[0]->IsObject()) {
        parent = ObjectWrap::Unwrap<UiWindow>(args[0]->ToObject());
    }

    _this->_parent = parent;
    _this->Show(rect);
}

void UiWindow::Close(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    UiWindow* _this = ObjectWrap::Unwrap<UiWindow>(args.This());
    _this->Close();
}

void UiWindow::Navigate(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    if (args.Length() == 0 || !args[0]->BooleanValue()) {
        return;
    }
    UiWindow* _this = ObjectWrap::Unwrap<UiWindow>(args.This());
    Utf8String* url = new Utf8String(args[0]);
    _this->Navigate(url);
}

void UiWindow::PostMsg(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    if (args.Length() == 0) {
        return;
    }
    UiWindow* _this = ObjectWrap::Unwrap<UiWindow>(args.This());
    auto str = JsonStringify(args[0]);
    Utf8String* msg = new Utf8String(str);
    Persistent<Function>* callback = NULL;
    if (args.Length() > 1 && args[1]->IsFunction())
        callback = new Persistent<Function>(isolate, Local<Function>::Cast(args[1]));
    _this->PostMsg(msg, callback);
}

void UiWindow::AsyncCallback(uv_async_t *handle) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    uv_mutex_lock(&_pendingEventsLock);
    WindowEventData* ev = _pendingEvents;
    _pendingEvents = NULL;
    uv_mutex_unlock(&_pendingEventsLock);

    while (ev) {
        ev->Sender->InvokeEventCallback(isolate, ev);
        WindowEventData* prev = ev;
        ev = ev->Next;
        delete prev;
    }
}

void UiWindow::InvokeEventCallback(Isolate* isolate, WindowEventData* data) {
    WINDOW_EVENT evt = data->Evt;

    Local<Object> hndl = handle();
    Local<Function> emitFn = Local<Function>::New(isolate, _emitFn);

    if (evt == WINDOW_EVENT_READY) {
        Handle<Value> argv[] = { String::NewFromUtf8(isolate, "ready") };
        emitFn->Call(hndl, 1, argv);
    }
    else if (evt == WINDOW_EVENT_CLOSING) {
        if (!_shouldClose) {
            auto closeArg = Object::New(isolate);
            Handle<Value> argv[] = { String::NewFromUtf8(isolate, "closing"), closeArg };
            auto result = emitFn->Call(hndl, 2, argv);
            result->BooleanValue();
            _shouldClose = !closeArg->Get(String::NewFromUtf8(isolate, "cancel"))->BooleanValue();
        }
        if (_shouldClose) {
            Close();
        }
    }
    else if (evt == WINDOW_EVENT_CLOSED) {
        Handle<Value> argv[] = { String::NewFromUtf8(isolate, "closed") };
        emitFn->Call(hndl, 1, argv);
    }
    else if (evt == WINDOW_EVENT_RESIZE) {
        Handle<Value> argv[] = { String::NewFromUtf8(isolate, "resize"),
            Int32::New(isolate, (int)data->Arg1), Int32::New(isolate, (int)data->Arg2) };
        emitFn->Call(hndl, 3, argv);
    }
    else if (evt == WINDOW_EVENT_STATE) {
        Handle<Value> argv[] = { String::NewFromUtf8(isolate, "state"), Int32::New(isolate, (int)data->Arg1) };
        emitFn->Call(hndl, 2, argv);
    }
    else if (evt == WINDOW_EVENT_MOVE) {
        Handle<Value> argv[] = { String::NewFromUtf8(isolate, "move"),
            Int32::New(isolate, (int)data->Arg1), Int32::New(isolate, (int)data->Arg2) };
        emitFn->Call(hndl, 3, argv);
    }
    else if (evt == WINDOW_EVENT_DOCUMENT_COMPLETE) {
        Handle<Value> argv[] = { String::NewFromUtf8(isolate, "documentComplete") };
        emitFn->Call(hndl, 1, argv);
    }
    else if (evt == WINDOW_EVENT_MENU) {
        UiMenu* menu = (UiMenu*)data->Arg1;
        Handle<Value> argId = menu->Id ? (Handle<Value>)*menu->Id : (Handle<Value>)Null(isolate);
        Handle<Value> argTitle = menu->Title ? (Handle<Value>)*menu->Title : (Handle<Value>)Null(isolate);
        Handle<Value> argv[] = { String::NewFromUtf8(isolate, "menu"), argId, argTitle };
        emitFn->Call(hndl, 3, argv);
        if (!menu->Callback.IsEmpty()) {
            Local<Function> fn = Local<Function>::New(isolate, menu->Callback);
            Handle<Value> callbackArgv[] = { argId, argTitle };
            fn->Call(hndl, 2, callbackArgv);
        }
    }
    else if (evt == WINDOW_EVENT_MESSAGE) {
        Utf8String* msg = (Utf8String*)data->Arg1;
        auto callback = (void*)data->Arg2;
        if (!_onMessageFn.IsEmpty()) {
            auto onMessage = Local<Function>::New(isolate, _onMessageFn);
            Handle<Value> argv[] = { JsonParse(*msg) };
            auto result = onMessage->Call(hndl, 1, argv);
            if (callback) {
                TryCatch tc;
                result = JsonStringify(result);
                Utf8String* resultStr = new Utf8String(result);
                Utf8String* errorStr = NULL;
                if (tc.HasCaught())
                    errorStr = new Utf8String(tc.Exception()->ToString());
                this->MsgCallback(callback, resultStr, errorStr);
            }
        }
        delete msg;
    }
    else if (evt == WINDOW_EVENT_MESSAGE_CALLBACK) {
        Persistent<Function>* callback = (Persistent<Function>*)data->Arg1;
        Utf8String* result = (Utf8String*)data->Arg2;
        Utf8String* error = (Utf8String*)data->Arg3;
        if (callback) {
            auto callbackFn = Local<Function>::New(isolate, *callback);
            Handle<Value> argv[] = { Null(isolate), Null(isolate) };
            if (result)
                argv[0] = JsonParse(*result);
            if (error)
                argv[0] = *error;
            callbackFn->Call(hndl, 2, argv);
            callback->Reset();
            delete callback;
        }
        if (result)
            delete result;
        if (error)
            delete error;
    }
    else if (evt == WINDOW_EVENT_SELECT_FILE) {
        auto params = (WindowOpenFileParams*)data->Arg1;
        auto result = (Utf8String**)data->Arg2;
        Handle<Value> callbackResult;
        if (result) {
            auto len = 0;
            for (len = 0; result[len]; ) {
                len++;
            }
            auto resArr = Array::New(isolate, len);
            callbackResult = resArr;
            for (auto i = 0; result[i]; i++) {
                resArr->Set(i, *result[i]);
                delete result[i];
            }
            delete[] result;
        } else {
            callbackResult = (Handle<Value>)Null(isolate);
        }
        if (!params->Complete.IsEmpty()) {
            auto callbackFn = Local<Function>::New(isolate, params->Complete);
            Handle<Value> argv[] = { callbackResult };
            callbackFn->Call(hndl, 1, argv);
        }
        delete params;
    }
}

void UiWindow::EmitEvent(WindowEventData* ev) {
    ev->Sender = this;
    uv_mutex_lock(&_pendingEventsLock);
    WindowEventData** ptr = &_pendingEvents;
    while (*ptr) {
        ptr = &(*ptr)->Next;
    }
    *ptr = ev;
    uv_mutex_unlock(&_pendingEventsLock);
    uv_async_send(&this->_uvAsyncHandle);
}

Handle<Value> UiWindow::JsonStringify(Handle<Value> val) {
    auto isolate = Isolate::GetCurrent();
    auto global = isolate->GetCurrentContext()->Global();
    auto JSON = global->Get(String::NewFromUtf8(isolate, "JSON"))->ToObject();
    auto fn = Handle<Function>::Cast(JSON->Get(String::NewFromUtf8(isolate, "stringify")));
    Handle<Value> argv[] = { val };
    return fn->Call(global, 1, argv);
}

Handle<Value> UiWindow::JsonParse(Handle<Value> val) {
    auto isolate = Isolate::GetCurrent();
    auto global = isolate->GetCurrentContext()->Global();
    auto JSON = global->Get(String::NewFromUtf8(isolate, "JSON"))->ToObject();
    auto fn = Handle<Function>::Cast(JSON->Get(String::NewFromUtf8(isolate, "parse")));
    Handle<Value> argv[] = { val };
    return fn->Call(global, 1, argv);
}

void UiWindow::SelectFile(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = ObjectWrap::Unwrap<UiWindow>(args.This());
    auto params = new WindowOpenFileParams();
    if (args.Length() > 0 && args[0]->IsObject()) {
        auto config = Handle<Object>::Cast(args[0]);
        params->Open = config->Get(String::NewFromUtf8(isolate, "open"))->BooleanValue();
        if (config->Get(String::NewFromUtf8(isolate, "title"))->IsString())
            params->Title = new Utf8String(config->Get(String::NewFromUtf8(isolate, "title")));
        params->Dirs = config->Get(String::NewFromUtf8(isolate, "dirs"))->BooleanValue();
        params->Multiple = config->Get(String::NewFromUtf8(isolate, "multiple"))->BooleanValue();
        if (config->Get(String::NewFromUtf8(isolate, "ext"))->IsArray()) {
            auto arr = Handle<Array>::Cast(config->Get(String::NewFromUtf8(isolate, "ext")));
            auto len = arr->Length();
            if (len > 0) {
                params->Ext = new Utf8String*[len + 1];
                for (unsigned i = 0; i < len; i++) {
                    params->Ext[i] = new Utf8String(arr->Get(i));
                }
                params->Ext[len] = NULL;
            }
        }
        if (config->Get(String::NewFromUtf8(isolate, "complete"))->IsFunction()) {
            auto fn = Local<Function>::Cast(config->Get(String::NewFromUtf8(isolate, "complete")));
            params->Complete.Reset(isolate, fn);
        }
    }
    if (!params->Title)
        params->Title = new Utf8String(String::NewFromUtf8(isolate, "Select file"));
    _this->SelectFile(params);
}

bool UiWindow::ShouldClose() {
    return _shouldClose;
}

void UiWindow::GetOnMessage(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    info.GetReturnValue().Set(_this->_onMessageFn);
}

void UiWindow::SetOnMessage(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    if (value->IsFunction()) {
        _this->_onMessageFn.Reset(isolate, Local<Function>::Cast(value));
    }
    else {
        _this->_onMessageFn.Reset();
    }
}

void UiWindow::GetWidth(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WindowRect rect = _this->GetWindowRect();
    info.GetReturnValue().Set(Int32::New(isolate, rect.Width));
}

void UiWindow::SetWidth(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WindowRect rect = _this->GetWindowRect();
    rect.Width = value->Int32Value();
    _this->SetWindowRect(rect);
}

void UiWindow::GetHeight(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WindowRect rect = _this->GetWindowRect();
    info.GetReturnValue().Set(Int32::New(isolate, rect.Height));
}

void UiWindow::SetHeight(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WindowRect rect = _this->GetWindowRect();
    rect.Height = value->Int32Value();
    _this->SetWindowRect(rect);
}

void UiWindow::GetLeft(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WindowRect rect = _this->GetWindowRect();
    info.GetReturnValue().Set(Int32::New(isolate, rect.Left));
}

void UiWindow::SetLeft(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WindowRect rect = _this->GetWindowRect();
    rect.Left = value->Int32Value();
    _this->SetWindowRect(rect);
}

void UiWindow::GetTop(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WindowRect rect = _this->GetWindowRect();
    info.GetReturnValue().Set(Int32::New(isolate, rect.Top));
}

void UiWindow::SetTop(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WindowRect rect = _this->GetWindowRect();
    rect.Top = value->Int32Value();
    _this->SetWindowRect(rect);
}

void UiWindow::Move(const FunctionCallbackInfo<Value>& args) {
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.Length() != 2 && args.Length() != 4) {
        isolate->ThrowException(String::NewFromUtf8(isolate, "arg"));
        return;
    }

    UiWindow* _this = Unwrap<UiWindow>(args.This());
    WindowRect rect = _this->GetWindowRect();

    if (args[0]->IsNumber()) {
        rect.Left = args[0]->Int32Value();
    }
    if (args[1]->IsNumber()) {
        rect.Top = args[1]->Int32Value();
    }
    if (args.Length() == 4) {
        if (args[2]->IsNumber()) {
            rect.Width = args[2]->Int32Value();
        }
        if (args[3]->IsNumber()) {
            rect.Height = args[3]->Int32Value();
        }
    }

    _this->SetWindowRect(rect);
}

void UiWindow::GetTitle(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    Utf8String* title = _this->GetTitle();
    info.GetReturnValue().Set((const Handle<Value>)*title);
    delete title;
}

void UiWindow::GetState(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    WINDOW_STATE state = _this->GetState();
    info.GetReturnValue().Set(Int32::New(isolate, state));
}

void UiWindow::SetState(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    if (value->IsNumber()) {
        WINDOW_STATE state = (WINDOW_STATE)value->Int32Value();
        _this->SetState(state);
    }
}

void UiWindow::GetResizable(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    bool resizable = _this->GetResizable();
    info.GetReturnValue().Set(Boolean::New(isolate, resizable));
}

void UiWindow::SetResizable(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    bool resizable = value->BooleanValue();
    _this->SetResizable(resizable);
}

void UiWindow::GetFrame(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    bool frame = _this->GetFrame();
    info.GetReturnValue().Set(Boolean::New(isolate, frame));
}

void UiWindow::SetFrame(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    bool frame = value->BooleanValue();
    _this->SetFrame(frame);
}

void UiWindow::GetTopmost(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    bool topmost = _this->GetTopmost();
    info.GetReturnValue().Set(Boolean::New(isolate, topmost));
}

void UiWindow::SetTopmost(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    bool topmost = value->BooleanValue();
    _this->SetTopmost(topmost);
}

void UiWindow::GetOpacity(Local<String> property, const PropertyCallbackInfo<Value>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    double opacity = _this->GetOpacity();
    info.GetReturnValue().Set(Number::New(isolate, opacity));
}

void UiWindow::SetOpacity(Local<String> property, Local<Value> value, const PropertyCallbackInfo<void>& info) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    UiWindow* _this = Unwrap<UiWindow>(info.This());
    double opacity = value->NumberValue();
    if (opacity < 0) {
        opacity = 0;
    }
    if (opacity > 1) {
        opacity = 1;
    }
    _this->SetOpacity(opacity);
}

void UiWindow::Alert(const FunctionCallbackInfo<Value>& args) {
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
    int ret = Alert(msg, type);
    args.GetReturnValue().Set(ret);
}

void UiWindow::ShowProgressDlg(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    if (args.Length() == 0) {
        return;
    }
    auto dlg = ProgressDialog::CreateNew(args);
    ShowProgressDlg(dlg);
}
