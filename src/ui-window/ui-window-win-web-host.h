#pragma once

#include "../utf8-string.h"
#include <Windows.h>

class IUiWindowWebHost {
public:
    virtual void Initialize() = 0;
    virtual void Destroy() = 0;
    virtual void SetSize(int width, int height) = 0;
    virtual void Navigate(LPCWSTR url) = 0;
    virtual bool HandleAccelerator(MSG* msg) = 0;
    virtual void PostMessageToBrowser(LPCWSTR json, void* callback) = 0;
    virtual void HandlePostMessageCallback(void* callback, LPCWSTR result, LPCWSTR error) = 0;
    virtual void Undo() = 0;
    virtual void Redo() = 0;
    virtual void Cut() = 0;
    virtual void Copy() = 0;
    virtual void Paste() = 0;
    virtual void SelectAll() = 0;
};

class IUiWindow {
public:
    virtual operator HWND() = 0;
    virtual void DownloadBegin() = 0;
    virtual void DownloadComplete() = 0;
    virtual void DocumentComplete() = 0;
    virtual void PostMessageToBackend(LPCWSTR msg, void* callback) = 0;
    virtual void HandlePostMessageCallback(void* callback, LPCWSTR result, LPCWSTR error) = 0;
    virtual LPCSTR GetEngineVersion() = 0;
};

IUiWindowWebHost* CreateCefWebHost(IUiWindow* window);
IUiWindowWebHost* CreateMsieWebHost(IUiWindow* window);
