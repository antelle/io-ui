#include <node.h>
#include <windows.h>
#include <Shlwapi.h>
#include <CommCtrl.h>
#include "ui-module.h"

using namespace v8;

void InitControls() {
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);
}

UI_RESULT SetIeControlBrowserVersion() {
    TCHAR buffer[MAX_PATH] = { 0 };
    DWORD bufSize = sizeof(buffer) / sizeof(*buffer);
    bool success = false;
    if (!GetModuleFileName(NULL, buffer, bufSize)) {
        return UI_E_INIT_MODULE;
    }
    TCHAR *exeName = PathFindFileName(buffer);
    HKEY key;
    if (RegCreateKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Internet Explorer\\Main\\FeatureControl\\FEATURE_BROWSER_EMULATION", &key) == ERROR_SUCCESS) {
        DWORD featureValue = 20000;
        success = RegSetValueEx(key, exeName, 0, REG_DWORD, (LPBYTE)&featureValue, sizeof(DWORD)) == ERROR_SUCCESS;
        RegCloseKey(key);
        return success ? UI_S_OK : UI_E_INIT_REG_VALUE;
    }
    else {
        return UI_E_INIT_REG_KEY;
    }
}

char* GetIeVersion() {
    bool success = false;
    HKEY key;
    if (RegOpenKey(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Internet Explorer", &key) == ERROR_SUCCESS) {
        DWORD type;
        char* data = new char[128];
        DWORD size = 256;
        LSTATUS result = RegQueryValueExA(key, "svcVersion", NULL, &type, (LPBYTE)data, &size);
        if (result != ERROR_SUCCESS) {
            result = RegQueryValueExA(key, "Version", NULL, &type, (LPBYTE)data, &size);
        }
        RegCloseKey(key);
        return result == ERROR_SUCCESS ? data : NULL;
    }
    return NULL;
}

UI_RESULT UiModule::OsInitialize() {
    OleInitialize(NULL);
    InitControls();

    UI_RESULT code = SetIeControlBrowserVersion();
    if (UI_FAILED(code)) {
        return code;
    }
    char* ieVersion = GetIeVersion();
    if (!ieVersion) {
        return UI_E_INIT_BROWSER_VERSION;
    }
    _engineName = "msie";
    _engineVersion = ieVersion;
    return UI_S_OK;
}
