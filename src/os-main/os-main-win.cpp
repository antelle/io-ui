#include "../ui-module/ui-module.h"
#include <windows.h>

extern int ExecCefSubprocessMain();

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    int argc;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv)
        return 0;
    char** argv = new char*[argc];
    for (int i = 0; i < argc; i++) {
        LPWSTR warg = wargv[i];
        // Compute the size of the required buffer
        DWORD size = WideCharToMultiByte(CP_UTF8, 0, warg, -1, NULL, 0, NULL, NULL);
        if (size == 0) {
            // This should never happen.
            fprintf(stderr, "Could not convert arguments to utf8.");
            exit(1);
        }
        // Do the actual conversion
        argv[i] = new char[size];
        DWORD result = WideCharToMultiByte(CP_UTF8, 0, warg, -1, argv[i], size, NULL, NULL);
        if (result == 0) {
            // This should never happen.
            fprintf(stderr, "Could not convert arguments to utf8.");
            exit(1);
        }
    }
    for (int i = 0; i < argc; i++) {
        if (!strncmp(argv[i], "--type=", 7)) {
            return ExecCefSubprocessMain();
        }
    }
    return UiModule::Main(argc, argv);
}
