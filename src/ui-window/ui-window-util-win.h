#pragma once

#define BASE_DPI 96

class UiWindowUtilWin {
public:
    static int GetDpi();
private:
    static int _dpi;
};
