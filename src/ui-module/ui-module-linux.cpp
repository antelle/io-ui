#include <node.h>
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <cstdio>
#include <string.h>
#include "ui-module.h"

using namespace v8;

UI_RESULT UiModule::OsInitialize() {
    _engineName = (char*)"webkitgtk";
    char ver[64];
    sprintf(ver, "%d.%d.%d", webkit_get_major_version(), webkit_get_minor_version(), webkit_get_micro_version());
    _engineVersion = new char[strlen(ver) + 1];
    strcpy(_engineVersion, ver);
    return UI_S_OK;
}

UI_RESULT UiModule::OsSetEngineVersion() {
}
