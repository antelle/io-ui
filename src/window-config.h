#pragma once

#include <node.h>
#include "const.h"
#include "utf8-string.h"

struct Coord {
    int Value;
    WINDOW_POSITION Position;

    Coord() : Coord(0, WINDOW_POSITION_AUTO) {}
    Coord(int value) : Coord(value, WINDOW_POSITION_PX) {}
    Coord(int value, WINDOW_POSITION position) : Value(value), Position(position) {};

    operator int() const {
        return Value;
    }
};

enum MENU_INTL : int {
    MENU_INTL_NULL = 0,
    MENU_INTL_FILE_EXIT = 10,
    MENU_INTL_EDIT_UNDO = 20,
    MENU_INTL_EDIT_REDO = 21,
    MENU_INTL_EDIT_CUT = 22,
    MENU_INTL_EDIT_COPY = 23,
    MENU_INTL_EDIT_PASTE = 24,
    MENU_INTL_EDIT_SELECT_ALL = 25,
    MENU_INTL_HELP_ABOUT = 30
};

struct UiMenu {
    MENU_TYPE Type = MENU_TYPE::MENU_TYPE_SIMPLE;
    Utf8String* Id = NULL;
    Utf8String* Title = NULL;
    v8::Persistent<v8::Function> Callback;
    UiMenu* Items = NULL;
    UiMenu* Next = NULL;
    UiMenu* Parent = NULL;
    enum MENU_INTL InternalMenuId = MENU_INTL_NULL;

    UiMenu() {}
    UiMenu(MENU_TYPE type) : Type(type) { }
    UiMenu(Utf8String* title) : Title(title) {}
    UiMenu(Utf8String* title, MENU_INTL internalMenuId) : Title(title), InternalMenuId(internalMenuId) {}

    ~UiMenu() {
        if (Title)
            delete Title;
        while (Items) {
            UiMenu* tmp = Items;
            Items = Items->Next;
            delete tmp;
        }
        Callback.Reset();
    }
};

struct WindowConfig {
    WindowConfig(v8::Handle<v8::Object> config);
    ~WindowConfig();

    Utf8String* Title;
    Coord Width;
    Coord Height;
    Coord Left;
    Coord Top;
    WINDOW_STATE State = WINDOW_STATE_NORMAL;
    bool Resizable = true;
    bool Frame = true;
    bool Topmost = false;
    double Opacity = 1;
    UiMenu* Menu = NULL;

private:
    Coord ToCoord(v8::Handle<v8::Value> val);
    UiMenu* ParseMenu(v8::Isolate* isolate, v8::Handle<v8::Array> arr);
};
