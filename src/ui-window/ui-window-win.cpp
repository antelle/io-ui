#include "ui-window.h"
#include "../perf-trace/perf-trace.h"
#include "../ui-module/ui-module.h"
#include "ui-window-win-web-host.h"
#include "ui-window-util-win.h"
#include <Windows.h>
#include <Shlobj.h>
#include <include/cef_app.h>
#include <iostream>
#include <iomanip>

#define WM_USER_CREATE_WINDOW (WM_APP + 1)
#define WM_USER_SHOW_PROGRESS_DLG (WM_APP + 2)
#define WM_USER_NAVIGATE (WM_APP + 3)
#define WM_USER_POST_MSG (WM_APP + 4)
#define WM_USER_POST_MSG_CB (WM_APP + 5)
#define WM_USER_SELECT_FILE (WM_APP + 6)

struct MsgCallbackParam {
    void* Callback;
    Utf8String* Result;
    Utf8String* Error;
};

class UiWindowWin : public UiWindow, public IUiWindow {
protected:
    virtual void Show(WindowRect& rect);
    virtual void Close();
    virtual void Navigate(Utf8String* url);
    virtual void PostMsg(Utf8String* msg, v8::Persistent<v8::Function>* callback);
    virtual void MsgCallback(void* callback, Utf8String* result, Utf8String* error);
    virtual void SelectFile(WindowOpenFileParams* params);

    virtual void SetWindowRect(WindowRect& rect);
    virtual WindowRect GetWindowRect();
    virtual WindowRect GetScreenRect();
    virtual Utf8String* GetTitle();
    virtual WINDOW_STATE GetState();
    virtual void SetState(WINDOW_STATE state);
    virtual bool GetResizable();
    virtual void SetResizable(bool resizable);
    virtual bool GetFrame();
    virtual void SetFrame(bool frame);
    virtual bool GetTopmost();
    virtual void SetTopmost(bool frame);
    virtual double GetOpacity();
    virtual void SetOpacity(double opacity);
public:
    UiWindowWin();
    void ShowOsWindow();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HWND hwnd;
    static WCHAR _wndClassName[];
    static DWORD _mainThreadId;
    static bool _isCef;

    BOOL HandleMessage(UINT, WPARAM, LPARAM);
    bool HandleAccelerator(MSG* msg);
    void CreateWebHost();

    // IUiWindow
    virtual operator HWND();
    virtual void DownloadBegin();
    virtual void DownloadComplete();
    virtual void DocumentComplete();
    virtual void PostMessageToBackend(LPCWSTR msg, void* callback);
    virtual void HandlePostMessageCallback(void* callback, LPCWSTR result, LPCWSTR error);
    virtual LPCSTR GetEngineVersion();

private:
    IUiWindowWebHost* _webHost;
    HMENU _hmenu = NULL;
    WindowRect _rect;
    bool _resizeMove = false;
    bool _fullscreen = false;
    LONG _styleBackup;
    LONG _exStyleBackup;
    WindowRect _sizeBackup;

    void CreateWindowMenu();
    void AddMenu(UiMenu* menu, HMENU parent);
    void AddFileMenuItems(HMENU parent, bool hasItems);
    void AddEditMenuItems(HMENU parent, bool hasItems);
    void AddHelpMenuItems(HMENU parent, bool hasItems);
    void HandleInternalMenu(MENU_INTL menu);

    void NavigateSync(Utf8String* url);
    void PostMsgSync(Utf8String* msg, long callback);
    void PostMsgCbSync(MsgCallbackParam* cb);
    void SelectFileSync(WindowOpenFileParams* params);
};

WCHAR UiWindowWin::_wndClassName[] = L"IoUiWnd";
DWORD UiWindowWin::_mainThreadId = 0;
bool UiWindowWin::_isCef = false;

int UiWindow::Main(int argc, char* argv[]) {
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) {
        //std::cerr << "MSG " << std::hex << std::setw(8) << msg.message << std::setw(12) << msg.lParam << std::setw(12) << msg.wParam << std::setw(12) << msg.hwnd << std::endl;
        if (msg.message == WM_USER_CREATE_WINDOW && msg.wParam) {
            UiWindowWin* window = (UiWindowWin*)msg.wParam;
            window->ShowOsWindow();
            continue;
        }
        if (msg.message == WM_USER_SHOW_PROGRESS_DLG && msg.wParam) {
            ProgressDialog* dlg = (ProgressDialog*)msg.wParam;
            dlg->ShowOsWindow();
            continue;
        }
        if (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST || msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) {
            HWND rootHwnd = GetAncestor(msg.hwnd, GA_ROOT);
            WCHAR cls[16];
            if (GetClassName(rootHwnd, cls, sizeof(cls) / sizeof(WCHAR)) && !lstrcmp(cls, UiWindowWin::_wndClassName)) {
                UiWindowWin* _this = (UiWindowWin*)GetWindowLongPtr(rootHwnd, GWL_USERDATA);
                if (_this && _this->HandleAccelerator(&msg))
                    continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (UiWindowWin::_isCef) {
            CefDoMessageLoopWork();
        }
    }
    if (UiWindowWin::_isCef) {
        CefShutdown();
    }
    return 0;
}

UiWindow* UiWindow::CreateUiWindow() {
    return new UiWindowWin();
}

void UiWindowWin::Show(WindowRect& rect) {
    _rect = rect;
    PostThreadMessage(_mainThreadId, WM_USER_CREATE_WINDOW, (WPARAM)this, 0);
}

UI_RESULT UiWindow::OsInitialize() {
    HINSTANCE hAppInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc =
    {
        sizeof(wc),
        0,
        UiWindowWin::WindowProc,
        0, 0,
        hAppInstance,
        LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        NULL,
        UiWindowWin::_wndClassName,
        LoadIcon(NULL, IDI_APPLICATION)
    };
    RegisterClassEx(&wc);
    UiWindowWin::_mainThreadId = GetCurrentThreadId();
    return UI_S_OK;
}

UiWindowWin::UiWindowWin() {
}

int UiWindow::Alert(Utf8String* msg, ALERT_TYPE type) {
    DWORD msgType = MB_ICONINFORMATION | MB_OK;
    if (type == ALERT_TYPE::ALERT_ERROR)
        msgType = MB_ICONERROR | MB_OK;
    else if (type == ALERT_TYPE::ALERT_QUESTION)
        msgType = MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON1;
    auto result = MessageBox(HWND_DESKTOP, *msg, L"Error", msgType);
    return ((type == ALERT_TYPE::ALERT_QUESTION) && (result == IDYES));
}

void UiWindow::ShowProgressDlg(ProgressDialog* dlg) {
    PostThreadMessage(UiWindowWin::_mainThreadId, WM_USER_SHOW_PROGRESS_DLG, (WPARAM)dlg, 0);
}

LRESULT CALLBACK UiWindowWin::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    UiWindowWin* _this;
    if (uMsg == WM_CREATE && (_this = (UiWindowWin*)(LPCREATESTRUCT(lParam))->lpCreateParams)) {
        SetWindowLongPtr(hwnd, GWL_USERDATA, (long)_this);
        _this->hwnd = hwnd;
    }
    else {
        _this = (UiWindowWin*)GetWindowLongPtr(hwnd, GWL_USERDATA);
    }

    BOOL fDoDef = !(_this && _this->HandleMessage(uMsg, wParam, lParam));

    return fDoDef ? DefWindowProc(hwnd, uMsg, wParam, lParam) : 0;
}

BOOL UiWindowWin::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_DESTROY:
        _webHost->Destroy();
        _webHost = NULL;
        SetWindowLongPtr(hwnd, GWL_USERDATA, (long)NULL);
        EmitEvent(new WindowEventData(WINDOW_EVENT_CLOSED));
        if (_isMainWindow)
            PostQuitMessage(0);
        if (_isCef)
            CefQuitMessageLoop();
        if (_parent)
            EnableWindow(((UiWindowWin*)_parent)->hwnd, TRUE);
        return TRUE;
    case WM_CLOSE:
        if (ShouldClose())
            DestroyWindow(hwnd);
        else
            EmitEvent(new WindowEventData(WINDOW_EVENT_CLOSING));
        return true;
    case WM_CREATE:
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_CREATE_WINDOW);
        CreateWebHost();
        PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_INIT_BROWSER_CONTROL);
        return TRUE;
    case WM_ENTERSIZEMOVE:
        _resizeMove = true;
        return TRUE;
    case WM_EXITSIZEMOVE:
        _resizeMove = false;
        return TRUE;
    case WM_SIZE: {
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        _webHost->SetSize(x, y);
        if (_resizeMove) {
            EmitEvent(new WindowEventData(WINDOW_EVENT_RESIZE, x, y));
        }
        else {
            int state = 0;
            EmitEvent(new WindowEventData(WINDOW_EVENT_STATE, state));
        }
        return TRUE;
    }
    case WM_MOVE: {
        int x = (int)(short)LOWORD(lParam);
        int y = (int)(short)HIWORD(lParam);
        EmitEvent(new WindowEventData(WINDOW_EVENT_MOVE, x, y));
        return TRUE;
    }
    case WM_MENUCOMMAND: {
        int ix = wParam;
        HMENU hMenu = (HMENU)lParam;
        MENUITEMINFO mii = { 0 };
        mii.fMask = MIIM_DATA;
        mii.cbSize = sizeof(mii);
        if (GetMenuItemInfo(hMenu, ix, TRUE, &mii) && mii.dwItemData) {
            UiMenu* uiMenu = (UiMenu*)mii.dwItemData;
            EmitEvent(new WindowEventData(WINDOW_EVENT_MENU, mii.dwItemData));
            if (uiMenu && uiMenu->InternalMenuId > 0) {
                HandleInternalMenu(uiMenu->InternalMenuId);
            }
            return TRUE;
        }
        return FALSE;
    }
    case WM_USER_NAVIGATE:
        NavigateSync((Utf8String*)wParam);
        return TRUE;
    case WM_USER_POST_MSG:
        PostMsgSync((Utf8String*)wParam, (long)lParam);
        return TRUE;
    case WM_USER_POST_MSG_CB:
        PostMsgCbSync((MsgCallbackParam*)wParam);
        return TRUE;
    case WM_USER_SELECT_FILE:
        SelectFileSync((WindowOpenFileParams*)wParam);
        return TRUE;
    }
    return FALSE;
}

void UiWindowWin::NavigateSync(Utf8String* url) {
    PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_CALL_NAVIGATE);
    _webHost->Navigate(*url);
    delete url;
}

void UiWindowWin::PostMsgSync(Utf8String* msg, long callback) {
    _webHost->PostMessageToBrowser(*msg, (void*)callback);
    delete msg;
}

void UiWindowWin::PostMsgCbSync(MsgCallbackParam* cb) {
    if (cb->Callback) {
        _webHost->HandlePostMessageCallback(cb->Callback, cb->Result ? (LPCWSTR)*cb->Result : NULL, cb->Error ? (LPCWSTR)*cb->Error : NULL);
    }
    if (cb->Result)
        delete cb->Result;
    if (cb->Error)
        delete cb->Error;
    delete cb;
}

void UiWindowWin::SelectFileSync(WindowOpenFileParams* params) {
    Utf8String** result = NULL;
    if (params->Dirs) {
        BROWSEINFO bi = { 0 };
        bi.hwndOwner = hwnd;
        WCHAR folderName[MAX_PATH];
        memset(folderName, 0, sizeof(folderName));
        bi.pszDisplayName = folderName;
        bi.lpszTitle = *params->Title;
        bi.ulFlags = BIF_DONTGOBELOWDOMAIN | BIF_EDITBOX | BIF_NEWDIALOGSTYLE | BIF_USENEWUI;
        auto pidl = SHBrowseForFolder(&bi);
        if (pidl) {
            if (SHGetPathFromIDList(pidl, folderName)) {
                result = new Utf8String*[2];
                result[0] = new Utf8String(folderName);
                result[1] = 0;
            }
        }
    } else {
        OPENFILENAME ofs = { 0 };
        ofs.lStructSize = sizeof(OPENFILENAME);
        ofs.hwndOwner = hwnd;
        if (params->Ext) {
            WCHAR filter[1024];
            memset(filter, 0, sizeof(filter));
            bool allowAllTypes = false;
            for (int i = 0; params->Ext[i]; i++) {
                if (!lstrlen(*params->Ext[i]) || !lstrcmp(*params->Ext[i], L"*")) {
                    allowAllTypes = true;
                } else {
                    if (filter[0])
                        lstrcat(filter, L";");
                    lstrcat(filter, L"*.");
                    lstrcat(filter, *params->Ext[i]);
                    if (!ofs.lpstrDefExt) {
                        ofs.lpstrDefExt = *params->Ext[i];
                    }
                }
            }
            if (filter[0]) {
                auto len = lstrlen(filter);
                lstrcpy(&filter[len + 1], filter);
                if (allowAllTypes) {
                    lstrcpy(&filter[len * 2 + 2], L"All Files");
                    lstrcpy(&filter[len * 2 + lstrlen(L"All Files") + 3], L"*.*");
                }
                ofs.lpstrFilter = filter;
            }
        }
        WCHAR selectedFile[1024 * 50];
        memset(selectedFile, 0, sizeof(selectedFile));
        ofs.lpstrFile = selectedFile;
        ofs.nMaxFile = sizeof(selectedFile) / sizeof(WCHAR);
        ofs.lpstrTitle = *params->Title;
        ofs.Flags = OFN_EXPLORER;
        if (params->Open) {
            ofs.Flags |= OFN_FILEMUSTEXIST;
            if (params->Multiple)
                ofs.Flags |= OFN_ALLOWMULTISELECT;
        }
        auto res = params->Open ? GetOpenFileName(&ofs) : GetSaveFileName(&ofs);
        if (res) {
            int firstIx = lstrlen(selectedFile) + 1;
            WCHAR fileName[1024];
            int count = 0;
            int ix = firstIx;
            while (selectedFile[ix]) {
                ix += lstrlen(&selectedFile[ix]) + 1;
                count++;
            }
            if (count == 0)
                count = 1;
            result = new Utf8String*[count + 1];
            int addedCount = 0;
            ix = firstIx;
            while (selectedFile[ix]) {
                lstrcpy(fileName, selectedFile);
                lstrcat(fileName, L"\\");
                lstrcat(fileName, &selectedFile[ix]);
                ix += lstrlen(&selectedFile[ix]) + 1;
                result[addedCount] = new Utf8String(fileName);
                addedCount++;
            }
            if (addedCount == 0)
                result[0] = new Utf8String(selectedFile);
            result[count] = NULL;
        }
    }
    EmitEvent(new WindowEventData(WINDOW_EVENT_SELECT_FILE, (long)params, (long)result));
}

bool UiWindowWin::HandleAccelerator(MSG* msg) {
    if (!_webHost)
        return false;
    return _webHost->HandleAccelerator(msg);
}

void UiWindowWin::ShowOsWindow() {
    DWORD style = WS_OVERLAPPEDWINDOW | WS_SIZEBOX;
    if (!_config->Resizable) style &= ~(WS_MAXIMIZEBOX | WS_SIZEBOX);
    if (!_config->Frame) style &= ~(WS_CAPTION);

    HWND parentHwnd = _parent ? ((UiWindowWin*)_parent)->hwnd : HWND_DESKTOP;
    HINSTANCE hinst = GetModuleHandle(NULL);
    CreateWindowMenu();
    hwnd = CreateWindowEx(0, _wndClassName, *_config->Title,
        style,
        this->_rect.Left * UiWindowUtilWin::GetDpi() / BASE_DPI, this->_rect.Top * UiWindowUtilWin::GetDpi() / BASE_DPI,
        this->_rect.Width * UiWindowUtilWin::GetDpi() / BASE_DPI, this->_rect.Height * UiWindowUtilWin::GetDpi() / BASE_DPI,
        parentHwnd, _hmenu, hinst, this);
    if (_parent)
        EnableWindow(parentHwnd, FALSE);

    UpdateWindow(hwnd);
    SetState(_config->State);
    if (_config->Opacity < 1)
        SetOpacity(_config->Opacity);
    if (_config->Topmost)
        SetTopmost(_config->Topmost);
    if (!_config->Frame)
        SetFrame(_config->Frame);
    EmitEvent(new WindowEventData(WINDOW_EVENT_READY));
}

void UiWindowWin::CreateWebHost() {
    _isCef = UiModule::IsCef();
    _webHost = _isCef ? CreateCefWebHost(this) : CreateMsieWebHost(this);
    _webHost->Initialize();
}

void UiWindowWin::Close(void) {
    SendMessage(this->hwnd, WM_CLOSE, 0, 0);
}

void UiWindowWin::Navigate(Utf8String* url) {
    if (!wcslen(*url))
        return;
    PostMessage(hwnd, WM_USER_NAVIGATE, (WPARAM)url, 0);
}

void UiWindowWin::PostMsg(Utf8String* msg, v8::Persistent<v8::Function>* callback) {
    PostMessage(hwnd, WM_USER_POST_MSG, (WPARAM)msg, (LPARAM)callback);
}

void UiWindowWin::MsgCallback(void* callback, Utf8String* result, Utf8String* error) {
    auto cb = new MsgCallbackParam();
    cb->Callback = callback;
    cb->Result = result;
    cb->Error = error;
    PostMessage(hwnd, WM_USER_POST_MSG_CB, (WPARAM)cb, 0);
}

void UiWindowWin::SelectFile(WindowOpenFileParams* params) {
    PostMessage(hwnd, WM_USER_SELECT_FILE, (WPARAM)params, 0);
}

void UiWindowWin::CreateWindowMenu() {
    if (_isMainWindow && _config->Menu) {
        _hmenu = CreateMenu();
        MENUINFO mi = { 0 };
        mi.cbSize = sizeof(mi);
        mi.fMask = MIM_STYLE;
        mi.dwStyle = MNS_NOTIFYBYPOS;
        SetMenuInfo(_hmenu, &mi);

        AddMenu(_config->Menu, _hmenu);
    }
}

void UiWindowWin::AddMenu(UiMenu* menu, HMENU parent) {
    bool isFileMenu = !menu->Parent && menu->Title && (!lstrcmpi(*menu->Title, L"File") || !lstrcmpi(*menu->Title, L"&File"));
    bool isEditMenu = !menu->Parent && menu->Title && (!lstrcmpi(*menu->Title, L"Edit") || !lstrcmpi(*menu->Title, L"&Edit"));
    bool isHelpMenu = !menu->Parent && menu->Title && (!lstrcmpi(*menu->Title, L"Help") || !lstrcmpi(*menu->Title, L"&Help")
        || !lstrcmpi(*menu->Title, L"?") || !lstrcmpi(*menu->Title, L"&?"));
    bool isStandardMenu = isFileMenu || isEditMenu || isHelpMenu;
    if (menu->Type == MENU_TYPE::MENU_TYPE_SEPARATOR) {
        AppendMenu(parent, MF_SEPARATOR, NULL, NULL);
    }
    else if (isStandardMenu || ((menu->Type == MENU_TYPE::MENU_TYPE_SIMPLE) && menu->Items)) {
        HMENU popupMenu = CreatePopupMenu();
        AppendMenu(parent, MF_STRING | MF_POPUP, (UINT_PTR)popupMenu, *menu->Title);
        MENUINFO mi = { 0 };
        mi.cbSize = sizeof(mi);
        mi.fMask = MIM_STYLE;
        mi.dwStyle = MNS_NOTIFYBYPOS;
        SetMenuInfo(popupMenu, &mi);
        if (isEditMenu) {
            AddEditMenuItems(popupMenu, menu->Items != NULL);
        }
        if (menu->Items) {
            AddMenu(menu->Items, popupMenu);
        }
        if (isHelpMenu) {
            AddHelpMenuItems(popupMenu, menu->Items != NULL);
        }
        if (isFileMenu) {
            AddFileMenuItems(popupMenu, menu->Items != NULL);
        }
    }
    else {
        MENUITEMINFO info = { 0 };
        info.cbSize = sizeof(info);
        info.fMask = MIIM_DATA | MIIM_STRING;
        info.cch = wcslen(*menu->Title);
        info.dwTypeData = *menu->Title;
        info.dwItemData = (ULONG_PTR)menu;
        InsertMenuItem(parent, -1, TRUE, &info);
    }
    if (menu->Next) {
        AddMenu(menu->Next, parent);
    }
}

void UiWindowWin::AddFileMenuItems(HMENU parent, bool hasItems) {
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String(L"E&xit"), MENU_INTL_FILE_EXIT), parent);
}

void UiWindowWin::AddEditMenuItems(HMENU parent, bool hasItems) {
    AddMenu(new UiMenu(new Utf8String(L"&Undo"), MENU_INTL_EDIT_UNDO), parent);
    AddMenu(new UiMenu(new Utf8String(L"&Redo"), MENU_INTL_EDIT_REDO), parent);
    AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String(L"Cu&t"), MENU_INTL_EDIT_CUT), parent);
    AddMenu(new UiMenu(new Utf8String(L"&Copy"), MENU_INTL_EDIT_COPY), parent);
    AddMenu(new UiMenu(new Utf8String(L"&Paste"), MENU_INTL_EDIT_PASTE), parent);
    AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String(L"Select &All"), MENU_INTL_EDIT_SELECT_ALL), parent);
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
}

void UiWindowWin::AddHelpMenuItems(HMENU parent, bool hasItems) {
    if (hasItems)
        AddMenu(new UiMenu(MENU_TYPE_SEPARATOR), parent);
    AddMenu(new UiMenu(new Utf8String(L"&About"), MENU_INTL_HELP_ABOUT), parent);
}

void UiWindowWin::HandleInternalMenu(MENU_INTL menu) {
    switch (menu) {
    case MENU_INTL_FILE_EXIT:
        Close();
        break;
    case MENU_INTL_EDIT_UNDO:
        _webHost->Undo();
        break;
    case MENU_INTL_EDIT_REDO:
        _webHost->Redo();
        break;
    case MENU_INTL_EDIT_CUT:
        _webHost->Cut();
        break;
    case MENU_INTL_EDIT_COPY:
        _webHost->Copy();
        break;
    case MENU_INTL_EDIT_PASTE:
        _webHost->Paste();
        break;
    case MENU_INTL_EDIT_SELECT_ALL:
        _webHost->SelectAll();
        break;
    case MENU_INTL_HELP_ABOUT:
        break;
    }
}

UiWindowWin::operator HWND() {
    return hwnd;
}

void UiWindowWin::DownloadBegin() {
    PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_1ST_REQUEST_SENT);
}

void UiWindowWin::DownloadComplete() {
    PerfTrace::Reg(UI_PERF_EVENT::UI_PERF_EVENT_LOAD_INDEX_HTML);
}

void UiWindowWin::DocumentComplete() {
    EmitEvent(new WindowEventData(WINDOW_EVENT_DOCUMENT_COMPLETE));
}

void UiWindowWin::PostMessageToBackend(LPCWSTR msg, void* callback) {
    auto msgStr = new Utf8String(msg);
    EmitEvent(new WindowEventData(WINDOW_EVENT_MESSAGE, (long)msgStr, (long)callback));
}

void UiWindowWin::HandlePostMessageCallback(void* callback, LPCWSTR result, LPCWSTR error) {
    if (callback) {
        Utf8String* resultStr = result ? new Utf8String(result) : NULL;
        Utf8String* errorStr = error ? new Utf8String(error) : NULL;
        EmitEvent(new WindowEventData(WINDOW_EVENT_MESSAGE_CALLBACK, (long)callback, (long)resultStr, (long)errorStr));
    }
}

LPCSTR UiWindowWin::GetEngineVersion() {
    return UiModule::GetEngineVersion();
}

WindowRect UiWindowWin::GetWindowRect() {
    RECT rect;
    ::GetWindowRect(hwnd, &rect);
    return WindowRect(rect.left * BASE_DPI / UiWindowUtilWin::GetDpi(), rect.top * BASE_DPI / UiWindowUtilWin::GetDpi(),
        (rect.right - rect.left) * BASE_DPI / UiWindowUtilWin::GetDpi(), (rect.bottom - rect.top) * BASE_DPI / UiWindowUtilWin::GetDpi());
}

WindowRect UiWindowWin::GetScreenRect() {
    RECT rect;
    HWND hDesktop = GetDesktopWindow();
    ::GetWindowRect(hDesktop, &rect);
    return WindowRect(rect.left * BASE_DPI / UiWindowUtilWin::GetDpi(), rect.top * BASE_DPI / UiWindowUtilWin::GetDpi(),
        (rect.right - rect.left) * BASE_DPI / UiWindowUtilWin::GetDpi(), (rect.bottom - rect.top) * BASE_DPI / UiWindowUtilWin::GetDpi());
}

void UiWindowWin::SetWindowRect(WindowRect& rect) {
    MoveWindow(hwnd, rect.Left * UiWindowUtilWin::GetDpi() / BASE_DPI, rect.Top * UiWindowUtilWin::GetDpi() / BASE_DPI,
        rect.Width * UiWindowUtilWin::GetDpi() / BASE_DPI, rect.Height * UiWindowUtilWin::GetDpi() / BASE_DPI, true);
}

Utf8String* UiWindowWin::GetTitle() {
    auto len = GetWindowTextLength(hwnd);
    WCHAR* title = new WCHAR[len + 1];
    int res = GetWindowText(hwnd, title, len + 1);
    if (!res) {
        delete[] title;
        return NULL;
    }
    return new Utf8String(title);
}

WINDOW_STATE UiWindowWin::GetState() {
    if (!IsWindowVisible(hwnd))
        return WINDOW_STATE::WINDOW_STATE_HIDDEN;
    if (_fullscreen)
        return WINDOW_STATE::WINDOW_STATE_FULLSCREEN;
    if (IsIconic(hwnd))
        return WINDOW_STATE::WINDOW_STATE_MINIMIZED;
    if (IsZoomed(hwnd))
        return WINDOW_STATE::WINDOW_STATE_MAXIMIZED;
    return WINDOW_STATE::WINDOW_STATE_NORMAL;
}

void UiWindowWin::SetState(WINDOW_STATE state) {
    if (_fullscreen && (state != WINDOW_STATE::WINDOW_STATE_FULLSCREEN)) {
        SetWindowLong(hwnd, GWL_STYLE, _styleBackup);
        SetWindowLong(hwnd, GWL_EXSTYLE, _exStyleBackup);
        _fullscreen = false;
        SetWindowPos(hwnd, NULL, _sizeBackup.Left, _sizeBackup.Top, _sizeBackup.Width, _sizeBackup.Height,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    switch (state) {
    case WINDOW_STATE::WINDOW_STATE_NORMAL:
        ShowWindow(hwnd, SW_SHOWNORMAL);
        break;
    case WINDOW_STATE::WINDOW_STATE_HIDDEN:
        ShowWindow(hwnd, SW_HIDE);
        break;
    case WINDOW_STATE::WINDOW_STATE_MAXIMIZED:
        ShowWindow(hwnd, SW_SHOWMAXIMIZED);
        break;
    case WINDOW_STATE::WINDOW_STATE_MINIMIZED:
        ShowWindow(hwnd, SW_SHOWMINIMIZED);
        break;
    case WINDOW_STATE::WINDOW_STATE_FULLSCREEN:
        ShowWindow(hwnd, SW_SHOWNORMAL);
        _sizeBackup = GetWindowRect();
        _styleBackup = GetWindowLong(hwnd, GWL_STYLE);
        _exStyleBackup = GetWindowLong(hwnd, GWL_EXSTYLE);
        SetWindowLong(hwnd, GWL_STYLE, _styleBackup & ~(WS_CAPTION | WS_THICKFRAME));
        SetWindowLong(hwnd, GWL_EXSTYLE, _exStyleBackup & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &monitorInfo);
        SetWindowPos(hwnd, NULL, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top, monitorInfo.rcMonitor.right, monitorInfo.rcMonitor.bottom,
            SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        _fullscreen = true;
        break;
    }
}

bool UiWindowWin::GetResizable() {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    return (dwStyle & WS_SIZEBOX) == WS_SIZEBOX;
}

void UiWindowWin::SetResizable(bool resizable) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (resizable) {
        dwStyle |= (WS_MAXIMIZEBOX | WS_SIZEBOX);
    }
    else {
        dwStyle &= ~(WS_MAXIMIZEBOX | WS_SIZEBOX);
    }
    SetWindowLong(hwnd, GWL_STYLE, dwStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

bool UiWindowWin::GetFrame() {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    return (dwStyle & WS_CAPTION) == WS_CAPTION;
}

void UiWindowWin::SetFrame(bool frame) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
    if (frame) {
        dwStyle |= (WS_CAPTION);
    }
    else {
        dwStyle &= ~(WS_CAPTION);
    }
    SetWindowLong(hwnd, GWL_STYLE, dwStyle);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

bool UiWindowWin::GetTopmost() {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    return (dwStyle & WS_EX_TOPMOST) == WS_EX_TOPMOST;
}

void UiWindowWin::SetTopmost(bool topmost) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (topmost) {
        dwStyle |= (WS_EX_TOPMOST);
    }
    else {
        dwStyle &= ~(WS_EX_TOPMOST);
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, dwStyle);
    SetWindowPos(hwnd, topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

double UiWindowWin::GetOpacity() {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if ((dwStyle & WS_EX_LAYERED) == 0) {
        return 1;
    }
    BYTE alpha;
    GetLayeredWindowAttributes(hwnd, NULL, &alpha, NULL);
    return alpha / 255.;
}

void UiWindowWin::SetOpacity(double opacity) {
    DWORD dwStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    bool opaque = opacity == 1;
    if (opaque) {
        dwStyle &= ~(WS_EX_LAYERED);
    }
    else {
        dwStyle |= (WS_EX_LAYERED);
    }
    SetWindowLong(hwnd, GWL_EXSTYLE, dwStyle);
    SetLayeredWindowAttributes(hwnd, NULL, (BYTE)(opacity * 255), LWA_ALPHA);
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}
