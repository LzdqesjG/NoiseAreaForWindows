#include "overlay.h"
#include <cstring>
#include <vector>

struct OverlayWin {
    HWND    hwnd = nullptr;
    HDC     dc = nullptr;
    HBITMAP bmp = nullptr;
    void*   bits = nullptr;
    int     w = 0, h = 0;
    int     x = 0, y = 0;
};

static std::vector<OverlayWin> g_wins;
static bool g_topmost = true;
static bool g_visible = false;

static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_NCHITTEST:     return HTTRANSPARENT;   // 鼠标穿透
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND:    return 1;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void releaseWin(OverlayWin& w) {
    if (w.dc)  { DeleteDC(w.dc); w.dc = nullptr; }
    if (w.bmp) { DeleteObject(w.bmp); w.bmp = nullptr; }
    w.bits = nullptr;
    w.w = w.h = 0;
}

bool Overlay_Init() {
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = L"NoiseAreaOverlay";
    RegisterClassExW(&wc);
    return true;
}

void Overlay_Shutdown() {
    for (auto& w : g_wins) {
        releaseWin(w);
        if (w.hwnd) DestroyWindow(w.hwnd);
    }
    g_wins.clear();
}

static bool sameTargets(const OverlayTarget* t, int n) {
    if ((int)g_wins.size() != n) return false;
    for (int i = 0; i < n; ++i) {
        const auto& w = g_wins[(size_t)i];
        if (w.x != t[i].x || w.y != t[i].y || w.w != t[i].w || w.h != t[i].h) return false;
    }
    return true;
}

void Overlay_SetTargets(const OverlayTarget* t, int n) {
    if (n < 0) n = 0;
    if (sameTargets(t, n)) return;

    for (auto& w : g_wins) {
        releaseWin(w);
        if (w.hwnd) DestroyWindow(w.hwnd);
    }
    g_wins.clear();
    g_visible = false;

    for (int i = 0; i < n; ++i) {
        if (t[i].w <= 0 || t[i].h <= 0) continue;
        OverlayWin w;
        w.x = t[i].x; w.y = t[i].y; w.w = t[i].w; w.h = t[i].h;

        w.hwnd = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            L"NoiseAreaOverlay", L"", WS_POPUP,
            w.x, w.y, w.w, w.h, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        if (!w.hwnd) continue;

        HDC sdc = GetDC(nullptr);
        w.dc = CreateCompatibleDC(sdc);
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w.w;
        bi.bmiHeader.biHeight = -w.h;   // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        w.bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &w.bits, nullptr, 0);
        ReleaseDC(nullptr, sdc);
        if (!w.bmp || !w.bits) { releaseWin(w); DestroyWindow(w.hwnd); continue; }
        SelectObject(w.dc, w.bmp);
        g_wins.push_back(w);
    }
}

void Overlay_Show(bool show) {
    g_visible = show;
    for (auto& w : g_wins) {
        if (!w.hwnd) continue;
        ShowWindow(w.hwnd, show ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (show && g_topmost)
            SetWindowPos(w.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }
}

void Overlay_SetTopmost(bool on) {
    g_topmost = on;
    for (auto& w : g_wins) {
        if (!w.hwnd) continue;
        SetWindowPos(w.hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void Overlay_KeepTopmost() {
    if (!g_topmost || !g_visible) return;
    for (auto& w : g_wins) {
        if (!w.hwnd || !IsWindowVisible(w.hwnd)) continue;
        SetWindowPos(w.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }
}

void Overlay_Render(const Grid& g, const std::vector<uint32_t>& cells,
                    const RECT& hole, bool holeValid) {
    if (g_wins.empty() || cells.empty()) return;
    HDC sdc = nullptr;
    for (auto& w : g_wins) {
        if (!w.bits) continue;
        expandCells(g, cells, (uint32_t*)w.bits, w.w, w.h, w.w);

        if (holeValid) {
            int x0 = hole.left - w.x, x1 = hole.right - w.x;
            int y0 = hole.top - w.y, y1 = hole.bottom - w.y;
            x0 = x0 < 0 ? 0 : x0;
            y0 = y0 < 0 ? 0 : y0;
            x1 = x1 > w.w ? w.w : x1;
            y1 = y1 > w.h ? w.h : y1;
            if (x0 < x1 && y0 < y1) {
                for (int y = y0; y < y1; ++y)
                    std::memset((uint32_t*)w.bits + (size_t)y * w.w + x0, 0,
                                (size_t)(x1 - x0) * sizeof(uint32_t));
            }
        }

        if (!sdc) sdc = GetDC(nullptr);
        POINT dstPt{ w.x, w.y };
        SIZE  dstSz{ w.w, w.h };
        POINT srcPt{ 0, 0 };
        BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(w.hwnd, sdc, &dstPt, &dstSz, w.dc, &srcPt, 0, &bf, ULW_ALPHA);
    }
    if (sdc) ReleaseDC(nullptr, sdc);
}
