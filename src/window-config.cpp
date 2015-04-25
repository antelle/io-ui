#include "window-config.h"
#include <iostream>
#include <string.h>

using namespace v8;

WindowConfig::WindowConfig(v8::Handle<v8::Object> config) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    Title = new Utf8String(config->Get(String::NewFromUtf8(isolate, "title")));
    Width = ToCoord(config->Get(String::NewFromUtf8(isolate, "width")));
    Height = ToCoord(config->Get(String::NewFromUtf8(isolate, "height")));
    Left = ToCoord(config->Get(String::NewFromUtf8(isolate, "left")));
    Top = ToCoord(config->Get(String::NewFromUtf8(isolate, "top")));
    if (config->Get(String::NewFromUtf8(isolate, "state"))->IsNumber())
        State = (WINDOW_STATE)config->Get(String::NewFromUtf8(isolate, "state"))->Int32Value();
    if (config->Get(String::NewFromUtf8(isolate, "frame"))->IsBoolean())
        Frame = config->Get(String::NewFromUtf8(isolate, "frame"))->BooleanValue();
    if (config->Get(String::NewFromUtf8(isolate, "resizable"))->IsBoolean())
        Resizable = config->Get(String::NewFromUtf8(isolate, "resizable"))->BooleanValue();
    if (config->Get(String::NewFromUtf8(isolate, "topmost"))->IsBoolean())
        Topmost = config->Get(String::NewFromUtf8(isolate, "topmost"))->BooleanValue();
    if (config->Get(String::NewFromUtf8(isolate, "opacity"))->IsNumber())
        Opacity = config->Get(String::NewFromUtf8(isolate, "opacity"))->NumberValue();
    auto menu = config->Get(String::NewFromUtf8(isolate, "menu"));
    if (menu->IsArray())
        Menu = ParseMenu(isolate, Handle<Array>::Cast(menu));
}

WindowConfig::~WindowConfig() {
    delete Title;
}

Coord WindowConfig::ToCoord(v8::Handle<v8::Value> val) {
    if (val->IsNumber()) {
        return Coord(val->Int32Value());
    }
    if (val->IsString()) {
        auto strVal = val->ToString();
        char* str = new char[strVal->Length()];
        val->ToString()->WriteUtf8(str);
        if (strcmp("center", str) == 0) {
            return Coord(0, WINDOW_POSITION_CENTERED);
        }
        int len = strVal->Length();
        if (str[len - 1] == '%') {
            return Coord(atoi(str), WINDOW_POSITION_PERCENT);
        }
        bool isDigit = true;
        for (int i = 0; i < len; i++) {
            if (!isdigit(str[i])) {
                isDigit = false;
                break;
            }
        }
        if (isDigit) {
            return Coord(atoi(str), WINDOW_POSITION_PX);
        }
    }
    return Coord(0, WINDOW_POSITION_AUTO);
}

UiMenu* WindowConfig::ParseMenu(Isolate* isolate, Handle<Array> arr) {
    unsigned len = arr->Length();
    if (!arr->Length())
        return NULL;
    UiMenu* result = NULL;
    UiMenu* current = NULL;
    for (unsigned ix = 0; ix < len; ix++) {
        auto item = Handle<Object>::Cast(arr->Get(ix));
        if (ix == 0) {
            result = current = new UiMenu();
        }
        else {
            current->Next = new UiMenu();
            current = current->Next;
        }
        auto title = item->Get(String::NewFromUtf8(isolate, "title"));
        if (title->IsString())
            current->Title = new Utf8String(title);
        auto id = item->Get(String::NewFromUtf8(isolate, "id"));
        if (id->IsString())
            current->Id = new Utf8String(id);
        current->Type = (MENU_TYPE)item->Get(String::NewFromUtf8(isolate, "type"))->Int32Value();
        auto callback = item->Get(String::NewFromUtf8(isolate, "callback"));
        if (callback->IsFunction())
            current->Callback.Reset(isolate, Handle<Function>::Cast(callback));
        auto items = item->Get(String::NewFromUtf8(isolate, "items"));
        if (items->IsArray())
            current->Items = ParseMenu(isolate, Handle<Array>::Cast(items));
        auto curItem = current->Items;
        while (curItem) {
            curItem->Parent = current;
            curItem = curItem->Next;
        }
    }
    return result;
}
