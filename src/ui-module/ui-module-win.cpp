#include <node.h>
#include <windows.h>
#include <Shlwapi.h>
#include <CommCtrl.h>
#include <include/cef_version.h>
#include "ui-module.h"
#include <iostream>

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

char* GetCefVersion() {
    char* ver = new char[32];
    itoa(cef_version_info(2/*CHROME_VERSION_MAJOR*/), ver, 10);
    strcat(ver, ".");
    itoa(cef_version_info(3/*CHROME_VERSION_MINOR*/), &ver[strlen(ver)], 10);
    strcat(ver, ".");
    itoa(cef_version_info(4/*CHROME_VERSION_BUILD*/), &ver[strlen(ver)], 10);
    strcat(ver, ".");
    itoa(cef_version_info(5/*CHROME_VERSION_PATCH*/), &ver[strlen(ver)], 10);
    return ver;
}

UI_RESULT UiModule::OsInitialize() {
    OleInitialize(NULL);
    InitControls();
    UI_RESULT code = SetIeControlBrowserVersion();
    if (UI_FAILED(code)) {
        return code;
    }
    return OsSetEngineVersion();
}

UI_RESULT UiModule::OsSetEngineVersion() {
    WCHAR path[MAX_PATH];
    auto res = GetModuleFileName(NULL, path, sizeof(path) / sizeof(WCHAR));
    auto lastSlash = wcsrchr(path, L'\\');
    bool isCef = false;
    if (lastSlash) {
        lastSlash++;
        lstrcpy(lastSlash, L"libcef.dll");
        if (PathFileExists(path)) {
            _engineName = "cef";
            _engineVersion = GetCefVersion();
            isCef = true;
        }
    }
    if (!isCef) {
        _engineName = "msie";
        _engineVersion = GetIeVersion();
    }
    if (!_engineVersion) {
        return UI_E_INIT_BROWSER_VERSION;
    }
    return UI_S_OK;
}
