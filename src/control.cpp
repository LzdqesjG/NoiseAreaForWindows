// 主控制窗口：菜单 / 标签页 / 自绘控件 / 预览编辑画布 / 教程
#include "app.h"
#include "overlay.h"
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 控件 id
// ---------------------------------------------------------------------------
enum : int {
    ID_TOOL_BRUSH = 100, ID_TOOL_RECT, ID_TOOL_FILL, ID_TOOL_ERASER,
    ID_INVERT = 110, ID_BRUSH_SZ,
    ID_CLEAR = 120, ID_SAVE, ID_LOAD, ID_RESET,
    ID_COLOR_EDGE = 140, ID_COLOR_FILL, ID_COLOR_GLOW, ID_COLOR_SPARK,
    ID_CHK_ENABLED = 150, ID_CHK_TOPMOST, ID_CHK_PLAYING,
    ID_BTN_SCREEN = 160, ID_BTN_THEME,
    ID_TAB0 = 200, ID_TAB1, ID_TAB2, ID_TAB3,
    ID_PREVIEW = 300,
    IDM_BASE = 4000,
    IDM_SAVE = IDM_BASE + 1, IDM_LOAD, IDM_CLEAR, IDM_RESET, IDM_QUIT,
    IDM_DARK = IDM_BASE + 11, IDM_LIGHT,
    IDM_LEFT = IDM_BASE + 21, IDM_RIGHT, IDM_SAME, IDM_SPAN,
    IDM_TUT = IDM_BASE + 31, IDM_ABOUT,
};

// 自绘控件通知
#define WM_APP_SLIDER (WM_APP + 2)
#define WM_APP_CHECK  (WM_APP + 3)

// 滑块编号
enum SliderId {
    SL_EDGE_OPACITY = 0, SL_GLOW, SL_EDGE_STRENGTH, SL_BLOCK,
    SL_FILL, SL_DISP_STR, SL_DISP_BLEND, SL_DISP_SPEED,
    SL_SPARK_HUE, SL_SPARK_OPACITY, SL_SHINE_SPEED, SL_SHINE_BRIGHT,
    SL_NOISE_STR, SL_NOISE_RADIUS, SL_NOISE_SMOOTH, SL_NOISE_EVO, SL_NOISE_DIR,
    SL_ANIM_SPEED,
    SL_COUNT
};

struct SDef {
    const wchar_t* name;
    float mn, mx, step;
    int page;     // 0 外观 1 内部 2 噪域 3 显示
    int fmt;      // 0 %.2f  1 %d  2 %.3f
};

static const SDef kSDefs[SL_COUNT] = {
    { L"边缘不透明度", 0.0f, 2.0f,  0.01f, 0, 0 },
    { L"光晕亮度",     0.0f, 3.0f,  0.01f, 0, 0 },
    { L"边缘强度",     1.0f, 6.0f,  1.0f,  0, 1 },
    { L"格子像素",     2.0f, 32.0f, 1.0f,  0, 1 },
    { L"填充强度",     0.0f, 1.5f,  0.01f, 1, 0 },
    { L"扭曲强度",     0.0f, 0.5f,  0.005f,1, 2 },
    { L"位移混合",     0.0f, 1.0f,  0.01f, 1, 2 },
    { L"位移速度",     0.0f, 5.0f,  0.01f, 1, 2 },
    { L"闪光色偏",     0.0f, 1.0f,  0.01f, 1, 2 },
    { L"闪光明度",     0.0f, 12.0f, 0.01f, 1, 2 },
    { L"扫光速度",     0.0f, 60.0f, 0.1f,  1, 1 },
    { L"扫光亮度",     0.0f, 0.5f,  0.005f,1, 2 },
    { L"噪声强度",     0.0f, 2.0f,  0.01f, 2, 0 },
    { L"噪声半径",     0.1f, 1.5f,  0.01f, 2, 2 },
    { L"噪声平滑度",   0.0f, 2.0f,  0.01f, 2, 0 },
    { L"噪声演化速度", 0.0f, 0.2f,  0.001f,2, 2 },
    { L"方向变化速度", 0.0f, 120.0f,0.1f,  2, 1 },
    { L"播放速度",     0.0f, 3.0f,  0.01f, 3, 0 },
};

// ---------------------------------------------------------------------------
// 主题
// ---------------------------------------------------------------------------
struct Palette {
    COLORREF bg, panel, panelHi, text, subtle, accent, track, tabBg;
};

static Palette g_pal;
static HFONT  g_font = nullptr;
static HFONT  g_fontBold = nullptr;
static HBRUSH g_brBg = nullptr, g_brPanel = nullptr;
static int    g_dpi = 96;

static void applyTheme(ThemeKind k) {
    if (k == ThemeKind::Light) {
        g_pal = { RGB(247, 247, 250), RGB(238, 238, 243), RGB(228, 228, 236),
                  RGB(30, 30, 36), RGB(110, 110, 120), RGB(198, 54, 58),
                  RGB(206, 206, 214), RGB(250, 250, 252) };
    } else {
        g_pal = { RGB(28, 28, 33), RGB(38, 38, 45), RGB(52, 52, 62),
                  RGB(228, 228, 234), RGB(140, 140, 150), RGB(255, 92, 96),
                  RGB(64, 64, 74), RGB(34, 34, 40) };
    }
    if (g_brBg) DeleteObject(g_brBg);
    if (g_brPanel) DeleteObject(g_brPanel);
    g_brBg = CreateSolidBrush(g_pal.bg);
    g_brPanel = CreateSolidBrush(g_pal.panel);
}

// ---------------------------------------------------------------------------
// 全局状态
// ---------------------------------------------------------------------------
static HWND g_hMain = nullptr;
static HWND g_hTabBtns[4] = {};
static int  g_tabPage = 0;
static HWND g_hPreview = nullptr;
static HWND g_hSliders[SL_COUNT] = {};
static HWND g_hToolBtns[4] = {};
static HWND g_hInvert = nullptr;
static HWND g_hBrushSz = nullptr;
static HWND g_hColorBtn[4] = {};
static HWND g_hChk[3] = {};
static HWND g_hBtnScreen = nullptr, g_hBtnTheme = nullptr;
static HWND g_hTut = nullptr;
static std::vector<HWND> g_pageCtl[4];
static std::vector<OverlayTarget> g_targets;
static int  g_tool = 0;
static int  g_brushCells = 3;
static double g_lastTick = 0.0;

static int S(int v) { return MulDiv(v, g_dpi, 96); }

// ===========================================================================
// 自绘滑块
// ===========================================================================
struct SliderData {
    float mn = 0, mx = 1, step = 0.01f;
    int   pos = 0, steps = 1;
    bool  drag = false;
    int   id = 0;
    int   fmt = 0;     // 0 %.2f  1 %d  2 %.3f
};

static void sliderText(SliderData* d, wchar_t* buf, int cch) {
    float v = d->mn + d->pos * d->step;
    if (d->fmt == 1) swprintf_s(buf, cch, L"%d", (int)std::lround(v));
    else if (d->fmt == 2) swprintf_s(buf, cch, L"%.3f", v);
    else swprintf_s(buf, cch, L"%.2f", v);
}

static int sliderPosFromX(SliderData* d, int x, int w) {
    int pad = S(8);
    float t = (float)(x - pad) / (float)std::max(1, w - pad * 2);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return (int)std::lround(t * d->steps);
}

static void sliderPaint(HWND h, HDC dc) {
    SliderData* d = (SliderData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!d) return;
    RECT rc;
    GetClientRect(h, &rc);
    HBRUSH bg = CreateSolidBrush(g_pal.bg);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    int pad = S(8);
    int valW = S(58);
    int trackH = S(5);
    int cy = (rc.bottom + rc.top) / 2;
    RECT track{ rc.left + pad, cy - trackH / 2, rc.right - pad - valW, cy + trackH / 2 };

    HBRUSH tb = CreateSolidBrush(g_pal.track);
    FillRect(dc, &track, tb);
    DeleteObject(tb);

    float t = d->steps > 0 ? (float)d->pos / d->steps : 0.0f;
    RECT fill = track;
    fill.right = track.left + (int)((track.right - track.left) * t);
    if (fill.right > fill.left) {
        HBRUSH fb = CreateSolidBrush(g_pal.accent);
        FillRect(dc, &fill, fb);
        DeleteObject(fb);
    }

    int tx = track.left + (int)((track.right - track.left) * t);
    int r = S(7);
    HBRUSH kb = CreateSolidBrush(g_pal.panelHi);
    HGDIOBJ oldBr = SelectObject(dc, kb);
    HPEN pen = CreatePen(PS_SOLID, S(2), g_pal.accent);
    HGDIOBJ oldPen = SelectObject(dc, pen);
    Ellipse(dc, tx - r, cy - r, tx + r, cy + r);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBr);
    DeleteObject(pen);
    DeleteObject(kb);

    wchar_t buf[32];
    sliderText(d, buf, 32);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, g_pal.text);
    HGDIOBJ oldFont = SelectObject(dc, g_font);
    RECT tr{ rc.right - valW - S(2), rc.top, rc.right - S(2), rc.bottom };
    DrawTextW(dc, buf, -1, &tr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(dc, oldFont);
}

static LRESULT CALLBACK SliderProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    SliderData* d = (SliderData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        d = (SliderData*)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        sliderPaint(h, dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!d) return 0;
        RECT rc; GetClientRect(h, &rc);
        d->drag = true;
        SetCapture(h);
        d->pos = sliderPosFromX(d, GET_X_LPARAM(lp), rc.right - rc.left);
        InvalidateRect(h, nullptr, FALSE);
        SendMessageW(GetParent(h), WM_APP_SLIDER, (WPARAM)d->id, 0);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!d || !d->drag) return 0;
        RECT rc; GetClientRect(h, &rc);
        int p = sliderPosFromX(d, GET_X_LPARAM(lp), rc.right - rc.left);
        if (p != d->pos) {
            d->pos = p;
            InvalidateRect(h, nullptr, FALSE);
            SendMessageW(GetParent(h), WM_APP_SLIDER, (WPARAM)d->id, 0);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        if (d) d->drag = false;
        ReleaseCapture();
        return 0;
    case WM_MOUSEWHEEL: {
        if (!d) return 0;
        int delta = GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1;
        d->pos = std::min(d->steps, std::max(0, d->pos + delta));
        InvalidateRect(h, nullptr, FALSE);
        SendMessageW(GetParent(h), WM_APP_SLIDER, (WPARAM)d->id, 0);
        return 0;
    }
    default: break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static float sliderValueOf(HWND h) {
    SliderData* d = (SliderData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!d) return 0.0f;
    return d->mn + d->pos * d->step;
}

static void sliderSetValue(HWND h, float v) {
    SliderData* d = (SliderData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!d) return;
    int p = (int)std::lround((v - d->mn) / d->step);
    d->pos = std::min(d->steps, std::max(0, p));
    InvalidateRect(h, nullptr, FALSE);
}

// ===========================================================================
// 自绘复选框
// ===========================================================================
struct CheckData { bool checked = false; int id = 0; };

static LRESULT CALLBACK CheckProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    CheckData* d = (CheckData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        d = (CheckData*)cs->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN:
        if (d) {
            d->checked = !d->checked;
            InvalidateRect(h, nullptr, FALSE);
            SendMessageW(GetParent(h), WM_APP_CHECK, (WPARAM)d->id, 0);
        }
        return 0;
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(nullptr, IDC_HAND));
        return TRUE;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc;
        GetClientRect(h, &rc);
        HBRUSH bg = CreateSolidBrush(g_pal.bg);
        FillRect(dc, &rc, bg);
        DeleteObject(bg);

        int box = S(14);
        int top = rc.top + (rc.bottom - rc.top - box) / 2;
        RECT br{ rc.left, top, rc.left + box, top + box };
        HBRUSH bbg = CreateSolidBrush(g_pal.panelHi);
        FillRect(dc, &br, bbg);
        DeleteObject(bbg);
        FrameRect(dc, &br, (HBRUSH)GetStockObject(GRAY_BRUSH));

        if (d && d->checked) {
            HPEN pen = CreatePen(PS_SOLID, S(2), g_pal.accent);
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, br.left + S(3), br.top + box / 2, nullptr);
            LineTo(dc, br.left + box / 2, br.bottom - S(3));
            LineTo(dc, br.right - S(3), br.top + S(3));
            SelectObject(dc, old);
            DeleteObject(pen);
        }
        wchar_t text[128]{};
        GetWindowTextW(h, text, 128);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, g_pal.text);
        HGDIOBJ oldFont = SelectObject(dc, g_font);
        RECT tr{ br.right + S(6), rc.top, rc.right, rc.bottom };
        DrawTextW(dc, text, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dc, oldFont);
        EndPaint(h, &ps);
        return 0;
    }
    default: break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static HWND mkSlider(HWND parent, int id, int x, int y, int w, int h,
                     float mn, float mx, float step, float val, int fmt) {
    SliderData* d = new SliderData();
    d->mn = mn; d->mx = mx; d->step = step;
    d->steps = std::max(1, (int)std::lround((mx - mn) / step));
    d->pos = std::min(d->steps, std::max(0, (int)std::lround((val - mn) / step)));
    d->id = id;
    d->fmt = fmt;
    HWND hw = CreateWindowExW(0, L"NASlider", L"", WS_CHILD | WS_VISIBLE,
                              S(x), S(y), S(w), S(h), parent, (HMENU)(INT_PTR)id,
                              GetModuleHandleW(nullptr), d);
    return hw;
}

static HWND mkCheck(HWND parent, int id, const wchar_t* text, bool checked, int x, int y, int w, int h) {
    CheckData* d = new CheckData();
    d->checked = checked;
    d->id = id;
    HWND hw = CreateWindowExW(0, L"NACheck", text, WS_CHILD | WS_VISIBLE,
                              S(x), S(y), S(w), S(h), parent, (HMENU)(INT_PTR)id,
                              GetModuleHandleW(nullptr), d);
    return hw;
}

static bool checkGet(HWND h) {
    CheckData* d = (CheckData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    return d && d->checked;
}
static void checkSet(HWND h, bool v) {
    CheckData* d = (CheckData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (d) { d->checked = v; InvalidateRect(h, nullptr, FALSE); }
}

// ===========================================================================
// 参数读写
// ===========================================================================
static float paramValue(int id) {
    const Params& p = g_app.p;
    switch (id) {
    case SL_EDGE_OPACITY:  return p.edgeOpacity;
    case SL_GLOW:          return p.glowIntensity;
    case SL_EDGE_STRENGTH: return (float)p.edgeStrength;
    case SL_BLOCK:         return (float)p.blockPx;
    case SL_FILL:          return p.fillOpacity;
    case SL_DISP_STR:      return p.displaceStrength;
    case SL_DISP_BLEND:    return p.displaceBlend;
    case SL_DISP_SPEED:    return p.displaceSpeed;
    case SL_SPARK_HUE:     return p.sparkHueShift;
    case SL_SPARK_OPACITY: return p.sparkOpacity;
    case SL_SHINE_SPEED:   return p.shineSpeed;
    case SL_SHINE_BRIGHT:  return p.shineBrightness;
    case SL_NOISE_STR:     return p.noiseStrength;
    case SL_NOISE_RADIUS:  return p.noiseRadius;
    case SL_NOISE_SMOOTH:  return p.noiseSmoothness;
    case SL_NOISE_EVO:     return p.noiseEvoSpeed;
    case SL_NOISE_DIR:     return p.noiseDirChange;
    case SL_ANIM_SPEED:    return p.animSpeed;
    }
    return 0.0f;
}

static void paramSet(int id, float v) {
    Params& p = g_app.p;
    switch (id) {
    case SL_EDGE_OPACITY:  p.edgeOpacity = v; break;
    case SL_GLOW:          p.glowIntensity = v; break;
    case SL_EDGE_STRENGTH: p.edgeStrength = (int)std::lround(v); break;
    case SL_BLOCK:
        p.blockPx = std::max(2, (int)std::lround(v));
        App_UpdateMaskSpace();
        break;
    case SL_FILL:          p.fillOpacity = v; break;
    case SL_DISP_STR:      p.displaceStrength = v; break;
    case SL_DISP_BLEND:    p.displaceBlend = v; break;
    case SL_DISP_SPEED:    p.displaceSpeed = v; break;
    case SL_SPARK_HUE:     p.sparkHueShift = v; break;
    case SL_SPARK_OPACITY: p.sparkOpacity = v; break;
    case SL_SHINE_SPEED:   p.shineSpeed = v; break;
    case SL_SHINE_BRIGHT:  p.shineBrightness = v; break;
    case SL_NOISE_STR:     p.noiseStrength = v; break;
    case SL_NOISE_RADIUS:  p.noiseRadius = v; break;
    case SL_NOISE_SMOOTH:  p.noiseSmoothness = v; break;
    case SL_NOISE_EVO:     p.noiseEvoSpeed = v; break;
    case SL_NOISE_DIR:     p.noiseDirChange = v; break;
    case SL_ANIM_SPEED:    p.animSpeed = v; break;
    }
    g_app.dirty = true;
}

static void updateValueLabel(int id) {
    if (id >= 0 && id < SL_COUNT && g_hSliders[id]) InvalidateRect(g_hSliders[id], nullptr, FALSE);
}

static void refreshAllSliders() {
    for (int i = 0; i < SL_COUNT; ++i) {
        if (g_hSliders[i]) {
            sliderSetValue(g_hSliders[i], paramValue(i));
            InvalidateRect(g_hSliders[i], nullptr, FALSE);
        }
    }
}

// ===========================================================================
// 预览画布
// ===========================================================================
struct PreviewState {
    HWND hwnd = nullptr;
    HDC  memdc = nullptr;
    HBITMAP bmp = nullptr;
    void* bits = nullptr;
    int  cw = 0, ch = 0;
    int  ox = 0, oy = 0, rw = 0, rh = 0;
    bool down = false;
    int  ax = 0, ay = 0;
    int  lx = 0, ly = 0;
    int  mx = -1, my = -1;
    bool hasMouse = false;
};
static PreviewState g_pv;

static void previewFree() {
    if (g_pv.memdc) { DeleteDC(g_pv.memdc); g_pv.memdc = nullptr; }
    if (g_pv.bmp)   { DeleteObject(g_pv.bmp); g_pv.bmp = nullptr; }
    g_pv.bits = nullptr;
    g_pv.cw = g_pv.ch = 0;
}

static void previewEnsureSurface(int cw, int ch) {
    if (cw == g_pv.cw && ch == g_pv.ch && g_pv.bits) return;
    previewFree();
    if (cw <= 0 || ch <= 0) return;
    HDC sdc = GetDC(nullptr);
    g_pv.memdc = CreateCompatibleDC(sdc);
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cw;
    bi.bmiHeader.biHeight = -ch;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    g_pv.bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &g_pv.bits, nullptr, 0);
    ReleaseDC(nullptr, sdc);
    if (!g_pv.bmp || !g_pv.bits) { previewFree(); return; }
    SelectObject(g_pv.memdc, g_pv.bmp);
    g_pv.cw = cw; g_pv.ch = ch;
}

static void previewLayout() {
    const int vw = g_app.grid.vw > 0 ? g_app.grid.vw : 1920;
    const int vh = g_app.grid.vh > 0 ? g_app.grid.vh : 1080;
    if (g_pv.cw <= 1 || g_pv.ch <= 1) return;
    float sc = std::min((float)g_pv.cw / vw, (float)g_pv.ch / vh);
    g_pv.rw = std::max(1, (int)(vw * sc));
    g_pv.rh = std::max(1, (int)(vh * sc));
    g_pv.ox = (g_pv.cw - g_pv.rw) / 2;
    g_pv.oy = (g_pv.ch - g_pv.rh) / 2;
}

static void previewRender() {
    if (!g_pv.bits) return;
    previewLayout();
    uint32_t* dst = (uint32_t*)g_pv.bits;
    const size_t total = (size_t)g_pv.cw * g_pv.ch;
    uint32_t bgc = (g_app.theme == ThemeKind::Light) ? 0xFFEDEDF2u : 0xFF14141Au;
    for (size_t i = 0; i < total; ++i) dst[i] = bgc;
    if (g_pv.rw > 0 && g_pv.rh > 0 && !g_app.cellColors.empty()) {
        // 先把遮罩渲染到临时缓冲，再按预乘 alpha 合到背景上（否则透明处会变黑）
        static std::vector<uint32_t> tmp;
        tmp.resize((size_t)g_pv.rw * g_pv.rh);
        expandCells(g_app.grid, g_app.cellColors, tmp.data(), g_pv.rw, g_pv.rh, g_pv.rw);
        const uint32_t br = (bgc >> 16) & 0xFF, bg2 = (bgc >> 8) & 0xFF, bb = bgc & 0xFF;
        for (int y = 0; y < g_pv.rh; ++y) {
            const uint32_t* srow = &tmp[(size_t)y * g_pv.rw];
            uint32_t* drow = dst + (size_t)(g_pv.oy + y) * g_pv.cw + g_pv.ox;
            for (int x = 0; x < g_pv.rw; ++x) {
                uint32_t s = srow[x];
                uint32_t a = s >> 24;
                if (a == 0) continue;
                uint32_t inv = 255 - a;
                uint32_t r = ((s >> 16) & 0xFFu) + (br * inv / 255u);
                uint32_t gg = ((s >> 8) & 0xFFu) + (bg2 * inv / 255u);
                uint32_t b = (s & 0xFFu) + (bb * inv / 255u);
                if (r > 255) r = 255;
                if (gg > 255) gg = 255;
                if (b > 255) b = 255;
                drow[x] = 0xFF000000u | (r << 16) | (gg << 8) | b;
            }
        }
    }
}

// 预览像素 → 遮罩坐标（遮罩空间左上角为 0,0）
static void previewToMask(int px, int py, float& sx, float& sy) {
    float fx = g_pv.rw > 0 ? (float)(px - g_pv.ox) / g_pv.rw : 0.0f;
    float fy = g_pv.rh > 0 ? (float)(py - g_pv.oy) / g_pv.rh : 0.0f;
    fx = fx < 0.0f ? 0.0f : (fx > 1.0f ? 1.0f : fx);
    fy = fy < 0.0f ? 0.0f : (fy > 1.0f ? 1.0f : fy);
    sx = fx * g_app.grid.vw;
    sy = fy * g_app.grid.vh;
}

static void previewToCell(int px, int py, int& cx, int& cy) {
    float sx, sy;
    previewToMask(px, py, sx, sy);
    g_app.grid.screenToCell((int)sx, (int)sy, cx, cy);
}

static int paintValue() {
    if (g_tool == 3) return 0;
    return checkGet(g_hInvert) ? 2 : 1;
}

static void previewPaintAt(int px, int py, bool first) {
    float sx, sy;
    previewToMask(px, py, sx, sy);
    const float radius = (float)g_brushCells * g_app.grid.blockPx;
    if (first) {
        g_app.grid.stampCircle(sx, sy, radius, paintValue());
    } else {
        float px0, py0;
        previewToMask(g_pv.lx, g_pv.ly, px0, py0);
        g_app.grid.strokeLine(px0, py0, sx, sy, radius, paintValue());
    }
    g_app.dirty = true;
}

static LRESULT CALLBACK PreviewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_pv.hwnd = hwnd;
        return 0;
    case WM_SIZE:
        previewEnsureSurface(LOWORD(lp), HIWORD(lp));
        previewLayout();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (g_pv.memdc && g_pv.bits) {
            BitBlt(dc, 0, 0, g_pv.cw, g_pv.ch, g_pv.memdc, 0, 0, SRCCOPY);

            RECT r{ g_pv.ox, g_pv.oy, g_pv.ox + g_pv.rw, g_pv.oy + g_pv.rh };
            HBRUSH frameBr = CreateSolidBrush(g_pal.track);
            FrameRect(dc, &r, frameBr);
            DeleteObject(frameBr);

            if (g_app.grid.gw > 0) {
                const int every = 32;
                HPEN pen = CreatePen(PS_SOLID, 1, g_pal.panel);
                HGDIOBJ old = SelectObject(dc, pen);
                for (int cx = every; cx < g_app.grid.gw; cx += every) {
                    int x = g_pv.ox + (int)((float)cx / g_app.grid.gw * g_pv.rw);
                    MoveToEx(dc, x, g_pv.oy, nullptr);
                    LineTo(dc, x, g_pv.oy + g_pv.rh);
                }
                for (int cy = every; cy < g_app.grid.gh; cy += every) {
                    int y = g_pv.oy + (int)((float)cy / g_app.grid.gh * g_pv.rh);
                    MoveToEx(dc, g_pv.ox, y, nullptr);
                    LineTo(dc, g_pv.ox + g_pv.rw, y);
                }
                SelectObject(dc, old);
                DeleteObject(pen);
            }

            if (g_pv.hasMouse) {
                int rpx = (int)((float)g_brushCells * g_app.grid.blockPx);
                int w = std::max(3, (int)((float)(rpx * 2) / g_app.grid.vw * g_pv.rw));
                int hh = std::max(3, (int)((float)(rpx * 2) / g_app.grid.vh * g_pv.rh));
                int x0 = g_pv.mx - w / 2, y0 = g_pv.my - hh / 2;
                HPEN pen = CreatePen(PS_SOLID, 1, g_tool == 3 ? RGB(65, 209, 138) : g_pal.accent);
                HGDIOBJ old = SelectObject(dc, pen);
                HGDIOBJ oldBr = SelectObject(dc, GetStockObject(NULL_BRUSH));
                Ellipse(dc, x0, y0, x0 + w, y0 + hh);
                SelectObject(dc, oldBr);
                SelectObject(dc, old);
                DeleteObject(pen);
            }

            if (g_pv.down && g_tool == 1) {
                int cx, cy;
                previewToCell(g_pv.mx, g_pv.my, cx, cy);
                int ax = std::min(g_pv.ax, cx), ay = std::min(g_pv.ay, cy);
                int bx = std::max(g_pv.ax, cx), by = std::max(g_pv.ay, cy);
                int x0 = g_pv.ox + (int)((float)ax * g_app.grid.blockPx / g_app.grid.vw * g_pv.rw);
                int y0 = g_pv.oy + (int)((float)ay * g_app.grid.blockPx / g_app.grid.vh * g_pv.rh);
                int x1 = g_pv.ox + (int)((float)(bx + 1) * g_app.grid.blockPx / g_app.grid.vw * g_pv.rw);
                int y1 = g_pv.oy + (int)((float)(by + 1) * g_app.grid.blockPx / g_app.grid.vh * g_pv.rh);
                HGDIOBJ oldBr = SelectObject(dc, GetStockObject(NULL_BRUSH));
                HPEN pen = CreatePen(PS_DASH, 1, g_pal.text);
                HGDIOBJ old = SelectObject(dc, pen);
                Rectangle(dc, x0, y0, x1, y1);
                SelectObject(dc, old);
                SelectObject(dc, oldBr);
                DeleteObject(pen);
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int px = GET_X_LPARAM(lp), py = GET_Y_LPARAM(lp);
        g_pv.mx = px; g_pv.my = py; g_pv.hasMouse = true;
        if (g_pv.down && (g_tool == 0 || g_tool == 3)) {
            previewPaintAt(px, py, false);
            g_pv.lx = px; g_pv.ly = py;
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetCapture(hwnd);
        g_pv.down = true;
        g_pv.mx = GET_X_LPARAM(lp); g_pv.my = GET_Y_LPARAM(lp);
        g_pv.hasMouse = true;
        int cx, cy;
        previewToCell(g_pv.mx, g_pv.my, cx, cy);
        if (g_tool == 1) {
            g_pv.ax = cx; g_pv.ay = cy;
            g_app.dirty = true;
        } else if (g_tool == 2) {
            g_app.grid.floodFill(cx, cy, paintValue());
            g_app.dirty = true;
        } else {
            g_pv.lx = g_pv.mx; g_pv.ly = g_pv.my;
            previewPaintAt(g_pv.mx, g_pv.my, true);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_RBUTTONDOWN: {
        int px = GET_X_LPARAM(lp), py = GET_Y_LPARAM(lp);
        float sx, sy;
        previewToMask(px, py, sx, sy);
        g_app.grid.stampCircle(sx, sy, (float)g_brushCells * g_app.grid.blockPx, 0);
        g_app.dirty = true;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_pv.down && g_tool == 1) {
            int cx, cy;
            previewToCell(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), cx, cy);
            g_app.grid.applyRect(g_pv.ax, g_pv.ay, cx, cy, paintValue());
            g_app.dirty = true;
        }
        g_pv.down = false;
        ReleaseCapture();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSELEAVE:
        g_pv.hasMouse = false;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ===========================================================================
// 教程窗口
// ===========================================================================
static const wchar_t* kTutorialText =
    L"噪域画笔 · 使用说明\r\n"
    L"\r\n"
    L"一、这是什么\r\n"
    L"把 Phigros/Phira 的「噪域（block-area）」效果搬到桌面上：一个置顶、鼠标可穿透的透明层，\r\n"
    L"直接盖在屏幕上；参数与 noisepainter.sling.cc.cd 那套官方 uniform 对齐。\r\n"
    L"\r\n"
    L"二、画噪域\r\n"
    L"1. 上方画布是整屏（或所选显示器）的缩略图，直接在它上面画。\r\n"
    L"2. 工具：画笔（拖）、矩形（拖框）、填充（连通区）、橡皮（左键拖 / 右键单点）。\r\n"
    L"3. 勾上「反相(减色)」画出来的区块会暗一档（网页里的 bit1 通道）。\r\n"
    L"4. 笔刷滑块 = 半径（单位：格）。\r\n"
    L"\r\n"
    L"三、参数\r\n"
    L"· 外观：边缘不透明度 / 光晕亮度 / 边缘强度（环带宽窄）/ 格子像素（最小方块边长）/ 四个颜色。\r\n"
    L"· 内部：填充强度 / 扭曲强度 / 位移混合 / 位移速度 / 闪光色偏 / 闪光明度 / 扫光速度 / 扫光亮度。\r\n"
    L"· 噪域：噪声强度（核心，0 = 均匀填充）、噪声半径（团块大小）、噪声平滑度、\r\n"
    L"  噪声演化速度、方向变化速度。\r\n"
    L"· 显示：显示器模式（仅左屏 / 仅右屏 / 左右同内容 / 左右跨区）、深浅主题、\r\n"
    L"  显示噪域 / 总在最前 / 循环播放、播放速度。\r\n"
    L"\r\n"
    L"四、显示器模式\r\n"
    L"· 仅左屏 / 仅右屏：只在某一台显示器上显示。\r\n"
    L"· 左右同内容：两台都显示同一份噪域（按各自分辨率缩放）。\r\n"
    L"· 左右跨区：把整个虚拟桌面当成一张画布，可以横跨两台显示器。\r\n"
    L"\r\n"
    L"五、文件\r\n"
    L"· config.json：参数与状态（含 firstLaunch）。删掉它再启动会重新进入新手引导。\r\n"
    L"· mask.dat：画好的噪域遮罩。\r\n"
    L"· 关窗口时自动保存；「恢复默认设置」会删掉 config.json 并立刻补一份默认值。\r\n"
    L"\r\n"
    L"六、菜单\r\n"
    L"· 文件：保存 / 读取 / 清空 / 恢复默认设置 / 退出\r\n"
    L"· 视图：深色 / 浅色、显示器模式\r\n"
    L"· 帮助：本教程 / 关于\r\n"
    L"\r\n"
    L"提示：控制面板在覆盖层上是挖空的，不会挡住自己的界面；面板最小化后噪域会铺满整屏。\r\n";

static LRESULT CALLBACK TutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_pal.text);
        SetBkColor(dc, g_pal.bg);
        return (LRESULT)g_brBg;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) { DestroyWindow(hwnd); return 0; }
        return 0;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        g_hTut = nullptr;
        return 0;
    case WM_ERASEBKGND: {
        HDC dc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_brBg);
        return 1;
    }
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void showTutorial(HWND owner) {
    if (g_hTut && IsWindow(g_hTut)) {
        SetForegroundWindow(g_hTut);
        return;
    }
    int w = S(760), h = S(620);
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    HWND hw = CreateWindowExW(WS_EX_TOOLWINDOW, L"NATutorial", L"噪域画笔 · 教程",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              (sw - w) / 2, (sh - h) / 2, w, h,
                              owner, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hw) return;
    g_hTut = hw;

    HWND ed = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", kTutorialText,
                              WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                              S(12), S(12), w - S(36), h - S(96),
                              hw, (HMENU)(INT_PTR)2, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(ed, WM_SETFONT, (WPARAM)g_font, TRUE);
    SendMessageW(ed, EM_SETSEL, 0, 0);

    HWND btn = CreateWindowExW(0, L"BUTTON", L"知道了", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                               w - S(120), h - S(70), S(96), S(30),
                               hw, (HMENU)(INT_PTR)1, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(btn, WM_SETFONT, (WPARAM)g_font, TRUE);

    ShowWindow(hw, SW_SHOW);
    UpdateWindow(hw);
}

// ===========================================================================
// 定时器
// ===========================================================================
static double nowSeconds() {
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}

static void refreshTargets() {
    App_UpdateMaskSpace();
    g_targets = App_OverlayTargets();
    if (!g_targets.empty())
        Overlay_SetTargets(g_targets.data(), (int)g_targets.size());
}

static void tick() {
    double t = nowSeconds();
    double dt = g_lastTick > 0.0 ? t - g_lastTick : 0.0;
    g_lastTick = t;
    if (dt < 0.0 || dt > 0.5) dt = 0.016;

    auto& a = g_app;
    if (a.p.playing) {
        a.timeSec += dt * a.p.animSpeed;
        if (a.timeSec > 3600.0) a.timeSec = std::fmod(a.timeSec, 3600.0);
        a.dirty = true;
    }
    if (!a.dirty) return;
    a.dirty = false;

    buildCellColors(a.grid, a.p, a.timeSec, a.cellColors);

    RECT panel{};
    bool panelValid = false;
    if (g_hMain && IsWindowVisible(g_hMain) && !IsIconic(g_hMain) && GetWindowRect(g_hMain, &panel))
        panelValid = true;

    if (a.p.enabled && !g_targets.empty()) {
        Overlay_Render(a.grid, a.cellColors, panel, panelValid);
        Overlay_Show(true);
    } else {
        Overlay_Show(false);
    }
    previewRender();
    if (g_hPreview) InvalidateRect(g_hPreview, nullptr, FALSE);
}

// ===========================================================================
// UI 构建
// ===========================================================================
static HWND mkStatic(const wchar_t* text, int x, int y, int w, int h, DWORD style = SS_LEFT) {
    HWND hWnd = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | style,
                               S(x), S(y), S(w), S(h), g_hMain, nullptr, nullptr, nullptr);
    SendMessageW(hWnd, WM_SETFONT, (WPARAM)g_font, TRUE);
    return hWnd;
}

static HWND mkButton(const wchar_t* text, int id, int x, int y, int w, int h,
                     DWORD style = BS_PUSHBUTTON, int page = -1) {
    HWND hWnd = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                               S(x), S(y), S(w), S(h), g_hMain, (HMENU)(INT_PTR)id, nullptr, nullptr);
    SendMessageW(hWnd, WM_SETFONT, (WPARAM)g_font, TRUE);
    if (page >= 0) g_pageCtl[page].push_back(hWnd);
    return hWnd;
}

static void buildUi() {
    const int W = 640;

    g_hPreview = CreateWindowExW(WS_EX_CLIENTEDGE, L"NAPPreview", L"",
                                 WS_CHILD | WS_VISIBLE, S(12), S(10), S(W - 24), S(330),
                                 g_hMain, (HMENU)(INT_PTR)ID_PREVIEW, nullptr, nullptr);

    // —— 工具行 ——
    const wchar_t* tools[4] = { L"画笔", L"矩形", L"填充", L"橡皮" };
    for (int i = 0; i < 4; ++i)
        g_hToolBtns[i] = mkButton(tools[i], ID_TOOL_BRUSH + i, 12 + i * 56, 348, 50, 26, BS_OWNERDRAW);

    g_hInvert = mkCheck(g_hMain, ID_INVERT, L"反相(减色)", false, 238, 349, 104, 24);
    mkStatic(L"笔刷", 350, 353, 40, 18);
    g_hBrushSz = mkSlider(g_hMain, ID_BRUSH_SZ, 392, 344, 200, 32, 1, 40, 1.0f, (float)g_brushCells, 1);

    // —— 编辑按钮行 ——
    mkButton(L"清空", ID_CLEAR, 12, 380, 84, 26);
    mkButton(L"保存", ID_SAVE, 102, 380, 84, 26);
    mkButton(L"读取", ID_LOAD, 192, 380, 84, 26);
    mkButton(L"恢复默认设置", ID_RESET, 282, 380, 120, 26);

    // —— 自绘标签页 ——
    const wchar_t* tabNames[4] = { L"外观", L"内部", L"噪域", L"显示" };
    for (int i = 0; i < 4; ++i)
        g_hTabBtns[i] = mkButton(tabNames[i], ID_TAB0 + i, 12 + i * 96, 412, 92, 28, BS_OWNERDRAW);

    // 每页内容
    int rowOfPage[4] = { 0, 0, 0, 0 };
    const int pageLeft = 24, pageTop = 452, rowH = 32;
    for (int id = 0; id < SL_COUNT; ++id) {
        const SDef& d = kSDefs[id];
        int page = d.page;
        int row = rowOfPage[page]++;
        int y = pageTop + row * rowH;

        HWND lbl = mkStatic(d.name, pageLeft + 4, y + 7, 108, 18);
        g_pageCtl[page].push_back(lbl);

        g_hSliders[id] = mkSlider(g_hMain, id, pageLeft + 112, y, 440, 30,
                                  d.mn, d.mx, d.step, paramValue(id), d.fmt);
        g_pageCtl[page].push_back(g_hSliders[id]);
    }

    // 外观页：颜色
    {
        const wchar_t* cnames[4] = { L"边缘", L"填充", L"光晕", L"闪光" };
        int y = pageTop + 4 * rowH + 8;
        HWND lbl = mkStatic(L"颜色", pageLeft + 4, y + 6, 60, 18);
        g_pageCtl[0].push_back(lbl);
        for (int i = 0; i < 4; ++i) {
            g_hColorBtn[i] = mkButton(cnames[i], ID_COLOR_EDGE + i, pageLeft + 70 + i * 116, y, 108, 30,
                                      BS_OWNERDRAW, 0);
        }
    }

    // 噪域页：说明
    {
        HWND note = mkStatic(L"噪声强度是噪域的核心：0 = 均匀填充，越大块噪越强。",
                             pageLeft + 6, pageTop + 5 * rowH + 10, 560, 20);
        g_pageCtl[2].push_back(note);
    }

    // 显示页
    {
        int y = pageTop + 40;
        g_hBtnScreen = mkButton(L"显示器：左右跨区", ID_BTN_SCREEN, pageLeft + 6, y, 220, 30, BS_PUSHBUTTON, 3);
        g_hBtnTheme = mkButton(L"主题：深色", ID_BTN_THEME, pageLeft + 240, y, 170, 30, BS_PUSHBUTTON, 3);
        y += 42;
        g_hChk[0] = mkCheck(g_hMain, ID_CHK_ENABLED, L"显示噪域", true, pageLeft + 6, y, 110, 24);
        g_hChk[1] = mkCheck(g_hMain, ID_CHK_TOPMOST, L"总在最前", true, pageLeft + 140, y, 110, 24);
        g_hChk[2] = mkCheck(g_hMain, ID_CHK_PLAYING, L"循环播放", true, pageLeft + 274, y, 110, 24);
        for (int i = 0; i < 3; ++i) g_pageCtl[3].push_back(g_hChk[i]);
        y += 44;
        HWND hint = mkStatic(L"显示器模式决定噪域画在哪块屏：仅左屏 / 仅右屏 / 左右同内容 / 左右跨区",
                             pageLeft + 6, y, 570, 20);
        g_pageCtl[3].push_back(hint);
        HWND hint2 = mkStatic(L"左右跨区时遮罩覆盖整块虚拟桌面，可以横跨两台显示器画。",
                              pageLeft + 6, y + 24, 570, 20);
        g_pageCtl[3].push_back(hint2);
    }

    mkStatic(L"提示：在画布上左键绘制、右键擦除；覆盖层鼠标穿透，点击会落到下面的窗口。",
             14, 724, 600, 20);

    // 只显示第 0 页
    for (int p = 1; p < 4; ++p)
        for (HWND h : g_pageCtl[p]) ShowWindow(h, SW_HIDE);
}

static void switchTab(int page) {
    g_tabPage = page;
    for (int p = 0; p < 4; ++p) {
        for (HWND h : g_pageCtl[p]) {
            if (h) ShowWindow(h, p == page ? SW_SHOW : SW_HIDE);
        }
        if (g_hTabBtns[p]) InvalidateRect(g_hTabBtns[p], nullptr, TRUE);
    }
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

// ===========================================================================
// 命令处理
// ===========================================================================
static void applyThemeAndRedraw(ThemeKind k) {
    g_app.theme = k;
    applyTheme(k);
    if (g_hBtnTheme) {
        SetWindowTextW(g_hBtnTheme, k == ThemeKind::Light ? L"主题：浅色" : L"主题：深色");
        InvalidateRect(g_hBtnTheme, nullptr, TRUE);
    }
    RedrawWindow(g_hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    g_app.dirty = true;
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

static void updateScreenButton() {
    wchar_t buf[64];
    swprintf_s(buf, L"显示器：%s", ScreenModeName(g_app.screenMode));
    SetWindowTextW(g_hBtnScreen, buf);
    InvalidateRect(g_hBtnScreen, nullptr, TRUE);
}

static void setScreenMode(ScreenMode m) {
    g_app.screenMode = m;
    updateScreenButton();
    refreshTargets();
    g_app.dirty = true;
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

static void resetAll() {
    int r = MessageBoxW(g_hMain,
        L"恢复默认设置会清空当前噪域与全部参数，并删除 config.json。\n之后会立刻写入一份默认配置，所以不会再弹教程。继续？",
        L"恢复默认设置", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;
    App_ResetDefaults();
    applyTheme(g_app.theme);
    refreshAllSliders();
    checkSet(g_hChk[0], g_app.p.enabled);
    checkSet(g_hChk[1], g_app.p.topmost);
    checkSet(g_hChk[2], g_app.p.playing);
    updateScreenButton();
    refreshTargets();
    g_app.dirty = true;
    RedrawWindow(g_hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

static void screenMenu(HWND owner) {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING | (g_app.screenMode == ScreenMode::Left ? MF_CHECKED : 0), IDM_LEFT, L"仅左屏");
    AppendMenuW(m, MF_STRING | (g_app.screenMode == ScreenMode::Right ? MF_CHECKED : 0), IDM_RIGHT, L"仅右屏");
    AppendMenuW(m, MF_STRING | (g_app.screenMode == ScreenMode::Same ? MF_CHECKED : 0), IDM_SAME, L"左右同内容");
    AppendMenuW(m, MF_STRING | (g_app.screenMode == ScreenMode::Span ? MF_CHECKED : 0), IDM_SPAN, L"左右跨区");
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, owner, nullptr);
    DestroyMenu(m);
}

static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hMain = hwnd;
        buildUi();
        switchTab(0);
        refreshAllSliders();
        checkSet(g_hChk[0], g_app.p.enabled);
        checkSet(g_hChk[1], g_app.p.topmost);
        checkSet(g_hChk[2], g_app.p.playing);
        updateScreenButton();
        SetWindowTextW(g_hBtnTheme, g_app.theme == ThemeKind::Light ? L"主题：浅色" : L"主题：深色");
        SetTimer(hwnd, 1, 16, nullptr);
        g_lastTick = nowSeconds();
        refreshTargets();
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        if (g_app.grid.empty() && g_app.grid.gw > 0) {
            int gw = g_app.grid.gw, gh = g_app.grid.gh;
            g_app.grid.applyRect(gw / 5, gh / 3, gw * 4 / 5, gh * 2 / 3, 1);
        }
        g_app.dirty = true;
        return 0;
    }
    case WM_TIMER:
        if (wp == 1) {
            tick();
            static int tickCount = 0;
            if ((++tickCount % 30) == 0) refreshTargets();
            if ((tickCount % 60) == 0) Overlay_KeepTopmost();
        }
        return 0;
    case WM_NOTIFY:
        return 0;
    case WM_APP_SLIDER: {
        int id = (int)wp;
        if (id == ID_BRUSH_SZ) {
            g_brushCells = std::max(1, (int)std::lround(sliderValueOf(g_hBrushSz)));
            InvalidateRect(g_hPreview, nullptr, FALSE);
            return 0;
        }
        if (id >= 0 && id < SL_COUNT) {
            paramSet(id, sliderValueOf(g_hSliders[id]));
            updateValueLabel(id);
            InvalidateRect(g_hPreview, nullptr, FALSE);
        }
        return 0;
    }
    case WM_APP_CHECK: {
        int id = (int)wp;
        if (id == ID_CHK_ENABLED) { g_app.p.enabled = checkGet(g_hChk[0]); g_app.dirty = true; }
        else if (id == ID_CHK_TOPMOST) {
            g_app.p.topmost = checkGet(g_hChk[1]);
            Overlay_SetTopmost(g_app.p.topmost);
            SetWindowPos(hwnd, g_app.p.topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        } else if (id == ID_CHK_PLAYING) { g_app.p.playing = checkGet(g_hChk[2]); g_app.dirty = true; }
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (id >= ID_TAB0 && id <= ID_TAB3) {
            switchTab(id - ID_TAB0);
            return 0;
        }
        if (id >= ID_TOOL_BRUSH && id <= ID_TOOL_ERASER) {
            g_tool = id - ID_TOOL_BRUSH;
            for (int i = 0; i < 4; ++i) InvalidateRect(g_hToolBtns[i], nullptr, TRUE);
            InvalidateRect(g_hPreview, nullptr, FALSE);
            return 0;
        }
        switch (id) {
        case ID_CLEAR:
        case IDM_CLEAR:
            g_app.grid.clear();
            g_app.dirty = true;
            InvalidateRect(g_hPreview, nullptr, FALSE);
            return 0;
        case ID_SAVE:
        case IDM_SAVE:
            App_SaveConfig();
            App_SaveMask();
            return 0;
        case ID_LOAD:
        case IDM_LOAD:
            App_LoadConfig();
            App_UpdateMaskSpace();
            App_LoadMask();
            refreshAllSliders();
            applyThemeAndRedraw(g_app.theme);
            checkSet(g_hChk[0], g_app.p.enabled);
            checkSet(g_hChk[1], g_app.p.topmost);
            checkSet(g_hChk[2], g_app.p.playing);
            updateScreenButton();
            refreshTargets();
            g_app.dirty = true;
            return 0;
        case ID_RESET:
        case IDM_RESET:
            resetAll();
            return 0;
        case IDM_QUIT:
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case IDM_DARK:
            applyThemeAndRedraw(ThemeKind::Dark);
            return 0;
        case IDM_LIGHT:
            applyThemeAndRedraw(ThemeKind::Light);
            return 0;
        case IDM_LEFT:  setScreenMode(ScreenMode::Left);  return 0;
        case IDM_RIGHT: setScreenMode(ScreenMode::Right); return 0;
        case IDM_SAME:  setScreenMode(ScreenMode::Same);  return 0;
        case IDM_SPAN:  setScreenMode(ScreenMode::Span);  return 0;
        case IDM_TUT:
            showTutorial(hwnd);
            return 0;
        case IDM_ABOUT:
            MessageBoxW(hwnd,
                L"噪域画笔（Windows 版）\n\n"
                L"算法参考 Phigros / Phira 官方 block-area 实现，\n"
                L"参数对齐 noisepainter.sling.cc.cd。\n\n"
                L"C++17 · Win32 · 无第三方依赖",
                L"关于", MB_OK | MB_ICONINFORMATION);
            return 0;
        case ID_BTN_SCREEN:
            screenMenu(hwnd);
            return 0;
        case ID_BTN_THEME:
            applyThemeAndRedraw(g_app.theme == ThemeKind::Light ? ThemeKind::Dark : ThemeKind::Light);
            return 0;
        default:
            break;
        }
        if (id >= ID_COLOR_EDGE && id <= ID_COLOR_SPARK) {
            COLORREF* target = nullptr;
            switch (id) {
            case ID_COLOR_EDGE:  target = &g_app.p.edgeColor; break;
            case ID_COLOR_FILL:  target = &g_app.p.fillColor; break;
            case ID_COLOR_GLOW:  target = &g_app.p.glowColor; break;
            case ID_COLOR_SPARK: target = &g_app.p.sparkColor; break;
            }
            if (target) {
                static COLORREF custom[16] = {};
                CHOOSECOLORW cc{};
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hwnd;
                cc.lpCustColors = custom;
                cc.rgbResult = *target;
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;
                if (ChooseColorW(&cc)) {
                    *target = cc.rgbResult;
                    g_app.dirty = true;
                    InvalidateRect(g_hPreview, nullptr, FALSE);
                    InvalidateRect((HWND)GetDlgItem(hwnd, id), nullptr, TRUE);
                }
            }
            return 0;
        }
        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
        if (!dis) return 0;
        wchar_t text[64]{};
        GetWindowTextW(dis->hwndItem, text, 64);

        const bool isTool = (dis->CtlID >= ID_TOOL_BRUSH && dis->CtlID <= ID_TOOL_ERASER);
        const bool isTab = (dis->CtlID >= ID_TAB0 && dis->CtlID <= ID_TAB3);
        const bool isColor = (dis->CtlID >= ID_COLOR_EDGE && dis->CtlID <= ID_COLOR_SPARK);

        bool selected = false;
        if (isTool) selected = (dis->CtlID - ID_TOOL_BRUSH) == g_tool;
        if (isTab)  selected = (dis->CtlID - ID_TAB0) == g_tabPage;

        HBRUSH b = CreateSolidBrush(selected ? g_pal.panelHi : g_pal.panel);
        FillRect(dis->hDC, &dis->rcItem, b);
        DeleteObject(b);

        if (isColor) {
            COLORREF c = RGB(120, 120, 130);
            if (dis->CtlID == ID_COLOR_EDGE) c = g_app.p.edgeColor;
            else if (dis->CtlID == ID_COLOR_FILL) c = g_app.p.fillColor;
            else if (dis->CtlID == ID_COLOR_GLOW) c = g_app.p.glowColor;
            else if (dis->CtlID == ID_COLOR_SPARK) c = g_app.p.sparkColor;
            RECT r = dis->rcItem;
            InflateRect(&r, -3, -3);
            HBRUSH cb = CreateSolidBrush(c);
            FillRect(dis->hDC, &r, cb);
            DeleteObject(cb);
            FrameRect(dis->hDC, &r, (HBRUSH)GetStockObject(BLACK_BRUSH));
        }

        HBRUSH fb = CreateSolidBrush(g_pal.track);
        FrameRect(dis->hDC, &dis->rcItem, fb);
        DeleteObject(fb);

        if (selected) {
            RECT r = dis->rcItem;
            if (isTab) {
                // 选中的标签：底部一条强调色横线
                RECT bar{ r.left + S(8), r.bottom - S(3), r.right - S(8), r.bottom - S(1) };
                HBRUSH ab = CreateSolidBrush(g_pal.accent);
                FillRect(dis->hDC, &bar, ab);
                DeleteObject(ab);
            } else {
                InflateRect(&r, -1, -1);
                HPEN pen = CreatePen(PS_SOLID, S(2), g_pal.accent);
                HGDIOBJ old = SelectObject(dis->hDC, pen);
                HGDIOBJ oldBr = SelectObject(dis->hDC, GetStockObject(NULL_BRUSH));
                Rectangle(dis->hDC, r.left, r.top, r.right, r.bottom);
                SelectObject(dis->hDC, oldBr);
                SelectObject(dis->hDC, old);
                DeleteObject(pen);
            }
        }

        SetBkMode(dis->hDC, TRANSPARENT);
        SetTextColor(dis->hDC, selected ? g_pal.text : g_pal.subtle);
        HGDIOBJ oldFont = SelectObject(dis->hDC, g_font);
        DrawTextW(dis->hDC, text, -1, (RECT*)&dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(dis->hDC, oldFont);
        return TRUE;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_brBg);
        RECT panel{ S(12), S(444), S(628), S(712) };
        HBRUSH fb = CreateSolidBrush(g_pal.track);
        FrameRect(dc, &panel, fb);
        DeleteObject(fb);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_pal.text);
        SetBkColor(dc, g_pal.bg);
        return (LRESULT)g_brBg;
    }
    case WM_ERASEBKGND: {
        HDC dc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_brBg);
        return 1;
    }
    case WM_DISPLAYCHANGE:
        refreshTargets();
        g_app.dirty = true;
        InvalidateRect(g_hPreview, nullptr, FALSE);
        return 0;
    case WM_CLOSE:
        App_SaveConfig();
        App_SaveMask();
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ===========================================================================
// 菜单
// ===========================================================================
static HMENU buildMenu() {
    HMENU mb = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_SAVE, L"保存");
    AppendMenuW(file, MF_STRING, IDM_LOAD, L"读取");
    AppendMenuW(file, MF_STRING, IDM_CLEAR, L"清空噪域");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_RESET, L"恢复默认设置");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_QUIT, L"退出");

    HMENU view = CreatePopupMenu();
    AppendMenuW(view, MF_STRING, IDM_DARK, L"深色主题");
    AppendMenuW(view, MF_STRING, IDM_LIGHT, L"浅色主题");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_STRING, IDM_LEFT, L"仅左屏");
    AppendMenuW(view, MF_STRING, IDM_RIGHT, L"仅右屏");
    AppendMenuW(view, MF_STRING, IDM_SAME, L"左右同内容");
    AppendMenuW(view, MF_STRING, IDM_SPAN, L"左右跨区");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, IDM_TUT, L"教程");
    AppendMenuW(help, MF_STRING, IDM_ABOUT, L"关于");

    AppendMenuW(mb, MF_POPUP, (UINT_PTR)file, L"文件");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)view, L"视图");
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)help, L"帮助");
    return mb;
}

// ===========================================================================
// 对外接口
// ===========================================================================
bool Control_Init(HINSTANCE inst) {
    int dpi = 96;
    {
        HDC dc = GetDC(nullptr);
        if (dc) { dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(nullptr, dc); }
    }
    g_dpi = dpi;

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_TAB_CLASSES };
    InitCommonControlsEx(&icc);

    applyTheme(g_app.theme);
    g_font = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_fontBold = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");

    auto reg = [&](WNDPROC proc, const wchar_t* cls, int cursor, HBRUSH br) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = proc;
        wc.hInstance = inst;
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(cursor));
        wc.hbrBackground = br;
        wc.lpszClassName = cls;
        RegisterClassExW(&wc);
    };
    reg(MainProc, L"NoiseAreaMain", 32512, g_brBg);      // IDC_ARROW
    reg(PreviewProc, L"NAPPreview", 32515, g_brBg);      // IDC_CROSS
    reg(SliderProc, L"NASlider", 32512, nullptr);
    reg(CheckProc, L"NACheck", 32512, nullptr);
    reg(TutProc, L"NATutorial", 32512, g_brBg);

    const int clientW = S(640), clientH = S(756);
    RECT rc{ 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, TRUE, WS_EX_TOPMOST);
    const int winW = rc.right - rc.left, winH = rc.bottom - rc.top;
    const int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int px = (sw - winW) / 2, py = (sh - winH) / 3;
    if (px < 0) px = 0;
    if (py < 0) py = 0;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_APPWINDOW, L"NoiseAreaMain", L"噪域画笔 · Windows 置顶噪域",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        px, py, winW, winH, nullptr, buildMenu(), inst, nullptr);
    if (!hwnd) return false;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
}

HWND Control_GetHwnd() { return g_hMain; }

void Control_ShowTutorial() { showTutorial(g_hMain); }

void Control_RefreshTargets() { refreshTargets(); }

void Control_ApplyTheme() { applyThemeAndRedraw(g_app.theme); }
