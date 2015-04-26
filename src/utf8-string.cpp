#include "utf8-string.h"
#include <string.h>
#include <node.h>

#ifdef _WIN32
#define UTF8STRCPY wcscpy
#define UTF8STRLEN wcslen
#else
#define UTF8STRCPY strcpy
#define UTF8STRLEN strlen
#endif

using namespace v8;

Utf8String::Utf8String(UTF8CHAR* val) {
    UTF8CHAR* s = new UTF8CHAR[UTF8STRLEN(val) + 1];
    UTF8STRCPY(s, val);
    _val = s;
}

Utf8String::Utf8String(const UTF8CHAR* val) {
    UTF8CHAR* s = new UTF8CHAR[UTF8STRLEN(val) + 1];
    UTF8STRCPY(s, val);
    _val = s;
}

Utf8String::Utf8String(const Utf8String& str) {
    UTF8CHAR* s = new UTF8CHAR[UTF8STRLEN(str._val) + 1];
    UTF8STRCPY(s, str._val);
    _val = s;
}

Utf8String::Utf8String(Handle<Value> val) {
    if (val->IsNull() || val->IsUndefined()) {
        _val = new UTF8CHAR[1]{0};
    } else {
#ifdef _WIN32
        String::Value str(val);
#else
        String::Utf8Value str(val);
#endif
        UTF8CHAR* s = new UTF8CHAR[str.length() + 1];
        UTF8STRCPY(s, (UTF8CHAR*)*str);
        _val = s;
    }
}

Utf8String::~Utf8String() {
    delete _val;
}

Utf8String::operator v8::Handle<v8::Value>() const {
#ifdef _WIN32
    return String::NewFromTwoByte(Isolate::GetCurrent(), (uint16_t*)_val);
#else
    return String::NewFromUtf8(Isolate::GetCurrent(), _val);
#endif
}

Utf8String::operator const UTF8CHAR*() const {
    return _val;
}

Utf8String::operator UTF8CHAR*() const {
    return (UTF8CHAR*)_val;
}
