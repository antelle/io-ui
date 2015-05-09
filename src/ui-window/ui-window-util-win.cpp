#include "ui-window-util-win.h"
#include <Windows.h>

int UiWindowUtilWin::_dpi = 0;

int UiWindowUtilWin::GetDpi() {
    if (!_dpi) {
        _dpi = BASE_DPI;
        HDC hdc = GetDC(NULL);
        if (hdc) {
            _dpi = GetDeviceCaps(hdc, LOGPIXELSX);
        }
    }
    return _dpi;
}
