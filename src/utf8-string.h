#pragma once

#include <node.h>

#ifdef _WIN32
#define UTF8CHAR wchar_t
#else
#define UTF8CHAR char
#endif

class Utf8String {
public:
    Utf8String(v8::Handle<v8::Value> val);
    Utf8String(UTF8CHAR* val);
    Utf8String(const Utf8String& str);
    ~Utf8String();

    operator v8::Handle<v8::Value>() const;
    operator const UTF8CHAR*() const;
    operator UTF8CHAR*() const;

private:
    const UTF8CHAR* _val = NULL;
};
