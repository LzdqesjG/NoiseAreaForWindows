#include "app.h"
#include "overlay.h"
#include <cstdio>

bool Control_Init(HINSTANCE inst);
HWND Control_GetHwnd();
void Control_ShowWizard();
void Control_RefreshTargets();
void Control_ApplyTheme();

// 崩溃时在 exe 同目录留一份记录，方便排查
static LONG WINAPI CrashFilter(EXCEPTION_POINTERS* ep) {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    wchar_t* slash = wcsrchr(path, L'\\');
    if (slash) wcscpy_s(slash + 1, MAX_PATH - (slash + 1 - path), L"crash.txt");
    FILE* f = nullptr;
    if (_wfopen_s(&f, path, L"a") == 0 && f) {
        fprintf(f, "code=0x%08lX addr=%p\n",
                (unsigned long)ep->ExceptionRecord->ExceptionCode,
                (void*)ep->ExceptionRecord->ExceptionAddress);
        fclose(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

int APIENTRY wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int) {
    SetUnhandledExceptionFilter(CrashFilter);
    // 逐显示器 DPI 感知：屏幕坐标 = 物理像素，覆盖层才能精确铺满
    if (HMODULE u = LoadLibraryW(L"user32.dll")) {
        typedef BOOL(WINAPI* PFN_SetCtx)(void*);
        auto setCtx = (PFN_SetCtx)GetProcAddress(u, "SetProcessDpiAwarenessContext");
        if (setCtx) setCtx((void*)-4);   // PER_MONITOR_AWARE_V2
        else SetProcessDPIAware();
    }

    App_LoadConfig();
    App_UpdateMaskSpace();
    App_LoadMask();

    if (!Control_Init(inst)) return 1;
    if (!Overlay_Init()) return 1;
    Control_RefreshTargets();
    Overlay_SetTopmost(g_app.p.topmost);
    Overlay_Show(g_app.p.enabled);

    if (g_app.firstLaunch) {
        Control_ShowWizard();       // 首次启动：先选语言，再看向导
    }

    g_app.dirty = true;

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    Overlay_Shutdown();
    return 0;
}
