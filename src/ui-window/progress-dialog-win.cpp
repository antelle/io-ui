#include "progress-dialog.h"
#include "ui-window-util-win.h"
#include <Windows.h>
#include <commctrl.h>

class ProgressDialogWin : public ProgressDialog {
public:
    virtual void ShowOsWindow();
    virtual void Close();
    virtual void SetValue(int value, Utf8String* description);
private:
    static void OsInit();
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK SetFont(HWND child, LPARAM font);

    void CreateControls();

    static bool _initialized;
    static LPCWSTR _className;

    HWND _hwnd;
    HBRUSH _bgBrush;
    HWND _hProgress;
    HWND _hLabel;
};

bool ProgressDialogWin::_initialized = false;
LPCWSTR ProgressDialogWin::_className = L"IoUiProgess";

ProgressDialog* ProgressDialog::CreateProgressDialog() {
    return new ProgressDialogWin();
}

void ProgressDialogWin::OsInit() {
    if (_initialized) {
        return;
    }
    HINSTANCE hAppInstance = GetModuleHandle(NULL);
    WNDCLASSEX wc =
    {
        sizeof(wc),
        0,
        WndProc,
        0, 0,
        hAppInstance,
        LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW),
        (HBRUSH)(COLOR_WINDOW + 1),
        NULL,
        _className,
        LoadIcon(NULL, IDI_APPLICATION)
    };
    RegisterClassEx(&wc);
    _initialized = true;
}

LRESULT CALLBACK ProgressDialogWin::WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    ProgressDialogWin* _this;
    if (uMsg == WM_CREATE && (_this = (ProgressDialogWin*)(LPCREATESTRUCT(lParam))->lpCreateParams)) {
        SetWindowLongPtr(hwnd, GWL_USERDATA, (long)_this);
        _this->_hwnd = hwnd;
    }
    else {
        _this = (ProgressDialogWin*)GetWindowLongPtr(hwnd, GWL_USERDATA);
    }
    switch (uMsg) {
    case WM_DESTROY:
        _this->_hwnd = NULL;
        _this->EmitClosedEvent();
        SetWindowLongPtr(hwnd, GWL_USERDATA, NULL);
        return 0;
    case WM_CREATE:
        _this->CreateControls();
        return 0;
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED) {
            SendMessage(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        auto bgCol = GetSysColor(COLOR_WINDOW);
        HDC hdcStatic = (HDC)wParam;
        SetTextColor(hdcStatic, RGB(0, 0, 0));
        SetBkColor(hdcStatic, bgCol);
        return (INT_PTR)_this->_bgBrush;
    }
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

void ProgressDialogWin::CreateControls() {
    RECT rect;
    GetClientRect(_hwnd, &rect);

    auto hInst = GetModuleHandle(NULL);

    auto padding = 20 * UiWindowUtilWin::GetDpi() / BASE_DPI;
    auto progressBarHeight = 25 * UiWindowUtilWin::GetDpi() / BASE_DPI;

    _hProgress = CreateWindowEx(0, PROGRESS_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        padding, padding, rect.right - rect.left - padding * 2, progressBarHeight,
        _hwnd, NULL, hInst, NULL);

    auto labelTop = 52 * UiWindowUtilWin::GetDpi() / BASE_DPI;
    _hLabel = CreateWindowEx(NULL, WC_STATIC, L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        padding, labelTop, rect.right - rect.left - padding * 2, padding,
        _hwnd, NULL, hInst, this);

    auto btnPadding = 10 * UiWindowUtilWin::GetDpi() / BASE_DPI;
    auto btnWidth = 100 * UiWindowUtilWin::GetDpi() / BASE_DPI;
    auto btnHeight = 24 * UiWindowUtilWin::GetDpi() / BASE_DPI;
    auto hWndButton = CreateWindowEx(NULL, WC_BUTTON, L"Cancel",
        WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
        (rect.right - rect.left - btnWidth) / 2, rect.bottom - rect.top - btnHeight - btnPadding,
        btnWidth, btnHeight,
        _hwnd, NULL, hInst, NULL);

    auto bgCol = GetSysColor(COLOR_WINDOW);
    _bgBrush = CreateSolidBrush(bgCol);
    SetClassLongPtr(_hwnd, GCLP_HBRBACKGROUND, (LONG)_bgBrush);

    EnumChildWindows(_hwnd, (WNDENUMPROC)SetFont, (LPARAM)GetStockObject(DEFAULT_GUI_FONT));

    SetValue(_value, _text);
    _text = NULL;
}

BOOL CALLBACK ProgressDialogWin::SetFont(HWND child, LPARAM font) {
    SendMessage(child, WM_SETFONT, font, true);
    return true;
}

void ProgressDialogWin::ShowOsWindow() {
    OsInit();
    RECT desktopRect;
    HWND hDesktop = GetDesktopWindow();
    GetWindowRect(hDesktop, &desktopRect);
    auto hInst = GetModuleHandle(NULL);
    int width = 400 * UiWindowUtilWin::GetDpi() / BASE_DPI;
    int height = 140 * UiWindowUtilWin::GetDpi() / BASE_DPI;
    _hwnd = CreateWindowEx(0, _className, *_title,
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        (desktopRect.right - desktopRect.left - width) / 2,
        (desktopRect.bottom - desktopRect.top - height) / 2,
        width, height,
        NULL, NULL, hInst, this);
    UpdateWindow(_hwnd);
    ShowWindow(_hwnd, SW_NORMAL);
}

void ProgressDialogWin::Close() {
    if (_hwnd) {
        SendMessage(_hwnd, WM_CLOSE, 0, 0);
    }
}

void ProgressDialogWin::SetValue(int value, Utf8String* description) {
    if (!_hwnd) {
        return;
    }
    auto style = GetWindowLong(_hProgress, GWL_STYLE);
    if (value >= 0) {
        if (value > 100) {
            value = 100;
        }
        if ((style & PBS_MARQUEE) == PBS_MARQUEE) {
            SetWindowLong(_hProgress, GWL_STYLE, style & ~PBS_MARQUEE);
            SendMessage(_hProgress, PBM_SETMARQUEE, FALSE, 0);
        }
        SendMessage(_hProgress, PBM_SETPOS, value, 0);
    }
    else {
        if ((style & PBS_MARQUEE) == 0) {
            SetWindowLong(_hProgress, GWL_STYLE, style | PBS_MARQUEE);
            SendMessage(_hProgress, PBM_SETMARQUEE, TRUE, 0);
        }
    }
    if (description) {
        SetWindowText(_hLabel, *description);
        delete description;
    }
    UpdateWindow(_hwnd);
}
