#include "progress-dialog.h"

using namespace v8;

Persistent<Function> ProgressDialog::_constructor;
uv_async_t ProgressDialog::_uvAsyncHandle;

void ProgressDialog::Init() {
    if (_constructor.IsEmpty()) {
        Isolate *isolate = Isolate::GetCurrent();
        HandleScope scope(isolate);
        Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
        tpl->SetClassName(String::NewFromUtf8(isolate, "UiProgressDialog"));
        tpl->InstanceTemplate()->SetInternalFieldCount(3);

        NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);
        NODE_SET_PROTOTYPE_METHOD(tpl, "setValue", SetValue);

        _constructor.Reset(isolate, tpl->GetFunction());

        uv_async_init(uv_default_loop(), &_uvAsyncHandle, ProgressDialog::AsyncCallback);
    }
}

ProgressDialog::ProgressDialog() {
}

ProgressDialog::~ProgressDialog() {
    if (_title)
        delete _title;
    _closed.Reset();
}

void ProgressDialog::New(const FunctionCallbackInfo<Value>& args) {
    CreateNew(args);
}

ProgressDialog* ProgressDialog::CreateNew(const FunctionCallbackInfo<Value>& args) {
    Init();
    Isolate *isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    if (args.IsConstructCall()) {
        auto _this = CreateProgressDialog();

        auto config = Handle<Object>::Cast(args[0]);
        _this->_title = new Utf8String(config->Get(String::NewFromUtf8(isolate, "title")));
        _this->_text = new Utf8String(config->Get(String::NewFromUtf8(isolate, "text")));
        _this->_value = config->Get(String::NewFromUtf8(isolate, "value"))->Int32Value();
        _this->_closed.Reset(isolate, Local<Function>::Cast(config->Get(String::NewFromUtf8(isolate, "closed"))));
        _this->Wrap(args.This());

        args.GetReturnValue().Set(args.This());
        return _this;
    }
    else {
        const int argc = 1;
        Local<Value> argv[argc] = { args[0] };
        auto inst = Local<Function>::New(isolate, _constructor)->NewInstance(argc, argv);
        args.GetReturnValue().Set(inst);
        auto _this = ObjectWrap::Unwrap<ProgressDialog>(inst);
        return _this;
    }
}

void ProgressDialog::Close(const FunctionCallbackInfo<Value>& args) {
    auto isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    auto _this = ObjectWrap::Unwrap<ProgressDialog>(args.This());
    _this->Close();
}

void ProgressDialog::SetValue(const FunctionCallbackInfo<Value>& args) {
    auto isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    auto _this = ObjectWrap::Unwrap<ProgressDialog>(args.This());
    auto value = args[0]->Int32Value();
    Utf8String* description = args.Length() > 1 ? new Utf8String(args[1]) : NULL;
    _this->SetValue(value, description);
}

void ProgressDialog::AsyncCallback(uv_async_t *handle) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    auto _this = (ProgressDialog*)handle->data;
    if (!_this->_closed.IsEmpty()) {
        auto fn = Local<Function>::New(isolate, _this->_closed);
        fn->Call(Undefined(isolate), 0, NULL);
    }
}

void ProgressDialog::EmitClosedEvent() {
    _uvAsyncHandle.data = this;
    uv_async_send(&_uvAsyncHandle);
}
