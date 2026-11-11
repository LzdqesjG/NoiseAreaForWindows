// 主控制窗口：菜单 / 标签 / 自绘控件 / 预览画布 / 播放向导
#include "app.h"
#include "overlay.h"
#include "lang.h"
#include <commctrl.h>
#include <commdlg.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
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
    IDM_EN, IDM_ZH,
    IDM_LEFT = IDM_BASE + 21, IDM_RIGHT, IDM_SAME, IDM_SPAN,
    IDM_WIZARD = IDM_BASE + 31, IDM_ABOUT,
};

#define WM_APP_SLIDER (WM_APP + 2)
#define WM_APP_CHECK  (WM_APP + 3)

// 滑块编号
enum SliderId {
    SL_EDGE_OPACITY = 0, SL_GLOW, SL_EDGE_STRENGTH, SL_BLOCK,
    SL_FILL, SL_DISP_STR, SL_DISP_BLEND, SL_DISP_SPEED,
    SL_SPARK_HUE, SL_SPARK_OPACITY, SL_SHINE_SPEED, SL_SHINE_BRIGHT,
    SL_NOISE_STR, SL_NOISE_RADIUS, SL_NOISE_SMOOTH, SL_NOISE_EVO, SL_NOISE_DIR,
    SL_EDGE_JITTER, SL_ANIM_SPEED,
    SL_COUNT
};

struct SDef {
    int nameId;      // StrId
    float mn, mx, step;
    int page;        // 0 外观 1 内部 2 噪域 3 显示
    int fmt;         // 0 %.2f  1 %d  2 %.3f
};

static const SDef kSDefs[SL_COUNT] = {
    { S_SL_EDGE_OPACITY,  0.0f, 2.0f,  0.01f,  0, 0 },
    { S_SL_GLOW,          0.0f, 3.0f,  0.01f,  0, 0 },
    { S_SL_EDGE_STRENGTH, 1.0f, 6.0f,  1.0f,   0, 1 },
    { S_SL_BLOCK,         2.0f, 32.0f, 1.0f,   0, 1 },
    { S_SL_FILL,          0.0f, 1.5f,  0.01f,  1, 0 },
    { S_SL_DISP_STR,      0.0f, 0.5f,  0.005f, 1, 2 },
    { S_SL_DISP_BLEND,    0.0f, 1.0f,  0.01f,  1, 2 },
    { S_SL_DISP_SPEED,    0.0f, 5.0f,  0.01f,  1, 2 },
    { S_SL_SPARK_HUE,     0.0f, 1.0f,  0.01f,  1, 2 },
    { S_SL_SPARK_OPACITY, 0.0f, 12.0f, 0.01f,  1, 2 },
    { S_SL_SHINE_SPEED,   0.0f, 60.0f, 0.1f,   1, 1 },
    { S_SL_SHINE_BRIGHT,  0.0f, 0.5f,  0.005f, 1, 2 },
    { S_SL_NOISE_STR,     0.0f, 20.0f, 0.01f,  2, 0 },
    { S_SL_NOISE_RADIUS,  0.1f, 1.5f,  0.01f,  2, 2 },
    { S_SL_NOISE_SMOOTH,  0.0f, 2.0f,  0.01f,  2, 0 },
    { S_SL_NOISE_EVO,     0.0f, 0.2f,  0.001f, 2, 2 },
    { S_SL_NOISE_DIR,     0.0f, 120.0f,0.1f,   2, 1 },
    { S_SL_EDGE_JITTER,   0.0f, 2.0f,  0.01f,  2, 2 },
    { S_SL_ANIM_SPEED,    0.0f, 3.0f,  0.01f,  3, 0 },
};

// ---------------------------------------------------------------------------
// 主题
// ---------------------------------------------------------------------------
struct Palette {
    COLORREF bg, card, card2, text, subtle, accent, track, border;
};

static Palette g_pal;
static HFONT  g_font = nullptr, g_fontB = nullptr, g_fontSm = nullptr;
static HBRUSH g_brBg = nullptr, g_brCard = nullptr;
static int    g_dpi = 96;

static void applyTheme(ThemeKind k) {
    if (k == ThemeKind::Light) {
        g_pal = { RGB(244, 244, 248), RGB(255, 255, 255), RGB(238, 238, 244),
                  RGB(28, 28, 34), RGB(108, 108, 120), RGB(216, 58, 66),
                  RGB(216, 216, 224), RGB(222, 222, 230) };
    } else {
        g_pal = { RGB(23, 23, 28), RGB(31, 31, 38), RGB(43, 43, 52),
                  RGB(236, 236, 242), RGB(150, 150, 162), RGB(255, 77, 87),
                  RGB(58, 58, 68), RGB(50, 50, 60) };
    }
    if (g_brBg) DeleteObject(g_brBg);
    g_brBg = CreateSolidBrush(g_pal.bg);
    if (g_brCard) DeleteObject(g_brCard);
    g_brCard = CreateSolidBrush(g_pal.card);
}

// ---------------------------------------------------------------------------
// 全局
// ---------------------------------------------------------------------------
static HWND g_hMain = nullptr;
static HWND g_hPreview = nullptr;
static HWND g_hTabBtns[4] = {};
static int  g_tabPage = 0;
static HWND g_hSliders[SL_COUNT] = {};
static HWND g_sliderLbl[SL_COUNT] = {};
static HWND g_hToolBtns[4] = {};
static HWND g_hInvert = nullptr;
static HWND g_hBrushSz = nullptr;
static HWND g_hColorBtn[4] = {};
static HWND g_hChk[3] = {};
static HWND g_hBtnScreen = nullptr, g_hBtnTheme = nullptr;
static HWND g_hWizard = nullptr;
static std::vector<HWND> g_pageCtl[4];
static std::vector<OverlayTarget> g_targets;
static int  g_tool = 0;
static int  g_brushCells = 3;
static double g_lastTick = 0.0;

static int S(int v) { return MulDiv(v, g_dpi, 96); }

static void ApplyLanguage();
static HMENU buildMenu();

// ---------------------------------------------------------------------------
// 绘制小工具
// ---------------------------------------------------------------------------
static void fillRound(HDC dc, const RECT& r, int rad, COLORREF c) {
    HRGN rg = CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + 1, rad, rad);
    HBRUSH b = CreateSolidBrush(c);
    FillRgn(dc, rg, b);
    DeleteObject(b);
    DeleteObject(rg);
}

static void frameRound(HDC dc, const RECT& r, int rad, COLORREF c) {
    HRGN rg = CreateRoundRectRgn(r.left, r.top, r.right + 1, r.bottom + 1, rad, rad);
    HBRUSH b = CreateSolidBrush(c);
    FrameRgn(dc, rg, b, 1, 1);
    DeleteObject(b);
    DeleteObject(rg);
}

static void drawText2(HDC dc, const wchar_t* s, RECT r, UINT fmt, COLORREF col, HFONT f) {
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, col);
    HGDIOBJ old = SelectObject(dc, f ? f : g_font);
    DrawTextW(dc, s, -1, &r, fmt);
    SelectObject(dc, old);
}

// ---------------------------------------------------------------------------
// 按钮 hover 跟踪（子类化）
// ---------------------------------------------------------------------------
static std::map<HWND, WNDPROC> g_btnOld;
static std::map<HWND, bool>    g_btnHover;

static LRESULT CALLBACK BtnSub(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!g_btnHover[h]) {
            g_btnHover[h] = true;
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, h, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(h, nullptr, FALSE);
        }
        break;
    case WM_MOUSELEAVE:
        g_btnHover[h] = false;
        InvalidateRect(h, nullptr, FALSE);
        break;
    default:
        break;
    }
    auto it = g_btnOld.find(h);
    if (it != g_btnOld.end())
        return CallWindowProcW(it->second, h, msg, wp, lp);
    return DefWindowProcW(h, msg, wp, lp);
}

// ===========================================================================
// 自绘滑块
// ===========================================================================
struct SliderData {
    float mn = 0, mx = 1, step = 0.01f;
    int   pos = 0, steps = 1;
    bool  drag = false;
    int   id = 0;
    int   fmt = 0;
};

static void sliderText(SliderData* d, wchar_t* buf, int cch) {
    float v = d->mn + d->pos * d->step;
    if (d->fmt == 1) swprintf_s(buf, cch, L"%d", (int)std::lround(v));
    else if (d->fmt == 2) swprintf_s(buf, cch, L"%.3f", v);
    else swprintf_s(buf, cch, L"%.2f", v);
}

static int sliderPosFromX(SliderData* d, int x, int w) {
    int pad = S(9);
    float t = (float)(x - pad) / (float)std::max(1, w - pad * 2);
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    return (int)std::lround(t * d->steps);
}

static void sliderPaint(HWND h, HDC dc) {
    SliderData* d = (SliderData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    if (!d) return;
    RECT rc;
    GetClientRect(h, &rc);
    HBRUSH bg = CreateSolidBrush(g_pal.card);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);

    const int pad = S(9);
    const int valW = S(56);
    const int th = S(4);
    const int cy = (rc.top + rc.bottom) / 2;

    RECT track{ rc.left + pad, cy - th / 2, rc.right - pad - valW, cy + th / 2 };
    fillRound(dc, track, th, g_pal.track);

    float t = d->steps > 0 ? (float)d->pos / (float)d->steps : 0.0f;
    RECT fill = track;
    fill.right = track.left + (int)((track.right - track.left) * t);
    if (fill.right > fill.left + 1) fillRound(dc, fill, th, g_pal.accent);

    int tx = track.left + (int)((track.right - track.left) * t);
    int r = S(7);
    HBRUSH kb = CreateSolidBrush(g_pal.bg);
    HGDIOBJ ob = SelectObject(dc, kb);
    HPEN pen = CreatePen(PS_SOLID, S(2), g_pal.accent);
    HGDIOBJ op = SelectObject(dc, pen);
    Ellipse(dc, tx - r, cy - r, tx + r, cy + r);
    SelectObject(dc, op);
    SelectObject(dc, ob);
    DeleteObject(pen);
    DeleteObject(kb);

    wchar_t buf[32];
    sliderText(d, buf, 32);
    RECT tr{ rc.right - valW, rc.top, rc.right - S(2), rc.bottom };
    drawText2(dc, buf, tr, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, g_pal.text, g_font);
}

static LRESULT CALLBACK SliderProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    SliderData* d = (SliderData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE:
        d = (SliderData*)((CREATESTRUCT*)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        sliderPaint(h, dc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (!d) return 0;
        d->drag = true;
        SetCapture(h);
        {
            RECT rc; GetClientRect(h, &rc);
            d->pos = sliderPosFromX(d, GET_X_LPARAM(lp), rc.right - rc.left);
        }
        InvalidateRect(h, nullptr, FALSE);
        SendMessageW(GetParent(h), WM_APP_SLIDER, (WPARAM)d->id, 0);
        return 0;
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
    default:
        break;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

static float sliderValueOf(HWND h) {
    SliderData* d = (SliderData*)GetWindowLongPtrW(h, GWLP_USERDATA);
    return d ? d->mn + d->pos * d->step : 0.0f;
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
    case WM_CREATE:
        d = (CheckData*)((CREATESTRUCT*)lp)->lpCreateParams;
        SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)d);
        return 0;
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
        HBRUSH bg = CreateSolidBrush(g_pal.card);
        FillRect(dc, &rc, bg);
        DeleteObject(bg);

        int box = S(15);
        int top = rc.top + (rc.bottom - rc.top - box) / 2;
        RECT br{ rc.left, top, rc.left + box, top + box };
        fillRound(dc, br, S(5), d && d->checked ? g_pal.accent : g_pal.card2);
        frameRound(dc, br, S(5), d && d->checked ? g_pal.accent : g_pal.border);
        if (d && d->checked) {
            HPEN pen = CreatePen(PS_SOLID, S(2), RGB(255, 255, 255));
            HGDIOBJ old = SelectObject(dc, pen);
            MoveToEx(dc, br.left + S(3), br.top + S(7), nullptr);
            LineTo(dc, br.left + S(6), br.bottom - S(4));
            LineTo(dc, br.right - S(3), br.top + S(4));
            SelectObject(dc, old);
            DeleteObject(pen);
        }
        wchar_t text[128]{};
        GetWindowTextW(h, text, 128);
        RECT tr{ br.right + S(7), rc.top, rc.right, rc.bottom };
        drawText2(dc, text, tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE, g_pal.text, g_font);
        EndPaint(h, &ps);
        return 0;
    }
    default:
        break;
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
    return CreateWindowExW(0, L"NASlider", L"", WS_CHILD | WS_VISIBLE,
                           S(x), S(y), S(w), S(h), parent, (HMENU)(INT_PTR)id,
                           GetModuleHandleW(nullptr), d);
}

static HWND mkCheck(HWND parent, int id, const wchar_t* text, bool checked, int x, int y, int w, int h) {
    CheckData* d = new CheckData();
    d->checked = checked;
    d->id = id;
    return CreateWindowExW(0, L"NACheck", text, WS_CHILD | WS_VISIBLE,
                           S(x), S(y), S(w), S(h), parent, (HMENU)(INT_PTR)id,
                           GetModuleHandleW(nullptr), d);
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
// 参数
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
    case SL_EDGE_JITTER:   return p.edgeJitter;
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
    case SL_EDGE_JITTER:   p.edgeJitter = v; break;
    case SL_ANIM_SPEED:    p.animSpeed = v; break;
    }
    g_app.dirty = true;
}

static void refreshAllSliders() {
    for (int i = 0; i < SL_COUNT; ++i)
        if (g_hSliders[i]) sliderSetValue(g_hSliders[i], paramValue(i));
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
    uint32_t bgc = (g_app.theme == ThemeKind::Light) ? 0xFFEDEDF2u : 0xFF111116u;
    for (size_t i = 0; i < total; ++i) dst[i] = bgc;
    if (g_pv.rw > 0 && g_pv.rh > 0 && !g_app.cellColors.empty()) {
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
            frameRound(dc, r, S(4), g_pal.border);

            if (g_app.grid.gw > 0) {
                const int every = 32;
                HPEN pen = CreatePen(PS_SOLID, 1, g_pal.card2);
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
                int w = std::max(4, (int)((float)(rpx * 2) / g_app.grid.vw * g_pv.rw));
                int hh = std::max(4, (int)((float)(rpx * 2) / g_app.grid.vh * g_pv.rh));
                int x0 = g_pv.mx - w / 2, y0 = g_pv.my - hh / 2;
                HPEN pen = CreatePen(PS_SOLID, 1, g_tool == 3 ? RGB(70, 210, 150) : g_pal.accent);
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
        float sx, sy;
        previewToMask(GET_X_LPARAM(lp), GET_Y_LPARAM(lp), sx, sy);
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
// 播放向导
// ===========================================================================
static void subBtn(HWND h);
static HWND g_wzTitle = nullptr, g_wzBody = nullptr;
static HWND g_wzLang[2] = {};
static HWND g_wzBack = nullptr, g_wzNext = nullptr;
static int  g_wzPage = 0;

static void wizardShowPage(HWND hwnd, int page) {
    g_wzPage = page;
    bool langPage = (page == 0);
    ShowWindow(g_wzTitle, SW_SHOW);
    SetWindowTextW(g_wzTitle, T(langPage ? S_WIZ_TITLE : S_WIZ_TUT_TITLE));
    ShowWindow(g_wzBody, langPage ? SW_HIDE : SW_SHOW);
    for (int i = 0; i < 2; ++i) ShowWindow(g_wzLang[i], langPage ? SW_SHOW : SW_HIDE);
    ShowWindow(g_wzBack, langPage ? SW_HIDE : SW_SHOW);
    SetWindowTextW(g_wzNext, T(langPage ? S_WIZ_NEXT : S_WIZ_START));
    if (!langPage) {
        SetWindowTextW(g_wzBody, T(S_TUT_TEXT));
        SendMessageW(g_wzBody, EM_SETSEL, 0, 0);
        SendMessageW(g_wzBody, EM_SCROLLCARET, 0, 0);
    }
    InvalidateRect(hwnd, nullptr, TRUE);
}

static void wizardFinish() {
    g_app.firstLaunch = false;
    g_app.language = g_lang;
    App_SaveConfig();
    ApplyLanguage();
    if (g_hWizard) DestroyWindow(g_hWizard);
}

static LRESULT CALLBACK WizardProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        g_hWizard = hwnd;
        g_wzTitle = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
                                    S(28), S(22), S(700), S(34), hwnd, nullptr, nullptr, nullptr);
        SendMessageW(g_wzTitle, WM_SETFONT, (WPARAM)g_fontB, TRUE);

        g_wzLang[0] = CreateWindowExW(0, L"BUTTON", L"简体中文", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                      S(60), S(150), S(300), S(96), hwnd, (HMENU)(INT_PTR)401,
                                      GetModuleHandleW(nullptr), nullptr);
        g_wzLang[1] = CreateWindowExW(0, L"BUTTON", L"English", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                      S(400), S(150), S(300), S(96), hwnd, (HMENU)(INT_PTR)402,
                                      GetModuleHandleW(nullptr), nullptr);
        for (int i = 0; i < 2; ++i) SendMessageW(g_wzLang[i], WM_SETFONT, (WPARAM)g_fontB, TRUE);

        g_wzBody = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                   WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                   S(24), S(70), S(712), S(410), hwnd, (HMENU)(INT_PTR)403,
                                   GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_wzBody, WM_SETFONT, (WPARAM)g_font, TRUE);

        g_wzBack = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                   S(476), S(494), S(120), S(38), hwnd, (HMENU)(INT_PTR)404,
                                   GetModuleHandleW(nullptr), nullptr);
        g_wzNext = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                   S(608), S(494), S(128), S(38), hwnd, (HMENU)(INT_PTR)405,
                                   GetModuleHandleW(nullptr), nullptr);
        SendMessageW(g_wzBack, WM_SETFONT, (WPARAM)g_font, TRUE);
        SendMessageW(g_wzNext, WM_SETFONT, (WPARAM)g_font, TRUE);
        for (int i = 0; i < 2; ++i) subBtn(g_wzLang[i]);
        subBtn(g_wzBack);
        subBtn(g_wzNext);

        wizardShowPage(hwnd, g_app.firstLaunch ? 0 : 1);
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_pal.text);
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)GetStockObject(HOLLOW_BRUSH);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_pal.text);
        SetBkColor(dc, g_pal.card);
        return (LRESULT)g_brCard;
    }
    case WM_ERASEBKGND: {
        HDC dc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_brBg);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_brBg);
        RECT card{ S(16), S(52), rc.right - S(16), rc.bottom - S(16) };
        fillRound(dc, card, S(14), g_pal.card);
        frameRound(dc, card, S(14), g_pal.border);
        // 顶部强调条
        RECT bar{ S(28), S(50), S(96), S(53) };
        fillRound(dc, bar, S(3), g_pal.accent);
        if (g_wzPage == 0) {
            RECT hint{ S(28), S(110), rc.right - S(28), S(136) };
            drawText2(dc, T(S_WIZ_LANG_HINT), hint, DT_LEFT | DT_SINGLELINE, g_pal.subtle, g_font);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lp;
        if (!dis) return 0;
        wchar_t text[128]{};
        GetWindowTextW(dis->hwndItem, text, 128);
        bool hover = g_btnHover[dis->hwndItem];
        bool langCard = (dis->CtlID == 401 || dis->CtlID == 402);
        bool primary = (dis->CtlID == 405);

        RECT r = dis->rcItem;
        COLORREF fillc = primary ? g_pal.accent : (hover ? g_pal.card2 : g_pal.card);
        if (langCard) fillc = hover ? g_pal.accent : g_pal.card2;
        fillRound(dis->hDC, r, S(langCard ? 12 : 9), fillc);
        frameRound(dis->hDC, r, S(langCard ? 12 : 9), primary ? g_pal.accent : g_pal.border);

        COLORREF tc = g_pal.text;
        if (primary) tc = RGB(255, 255, 255);
        if (langCard && hover) tc = RGB(255, 255, 255);
        drawText2(dis->hDC, text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, tc,
                  langCard ? g_fontB : g_font);
        return TRUE;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == 401 || id == 402) {           // 选语言
            g_lang = (id == 402) ? 1 : 0;
            g_app.language = g_lang;
            wizardShowPage(hwnd, 1);
            return 0;
        }
        if (id == 404) { wizardShowPage(hwnd, 0); return 0; }
        if (id == 405) { wizardFinish(); return 0; }
        return 0;
    }
    case WM_CLOSE:
        wizardFinish();
        return 0;
    case WM_DESTROY:
        g_hWizard = nullptr;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void showWizard(int page) {
    if (g_hWizard && IsWindow(g_hWizard)) {
        SetForegroundWindow(g_hWizard);
        wizardShowPage(g_hWizard, page);
        return;
    }
    RECT rc{ 0, 0, S(780), S(560) };
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE,
                       WS_EX_TOOLWINDOW | WS_EX_TOPMOST);
    const int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    g_wzPage = page;
    HWND hw = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"NAWizard", L"Noise Painter",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              (sw - w) / 2, (sh - h) / 2, w, h,
                              g_hMain, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hw) return;
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
// 控件创建
// ===========================================================================
static HWND mkStatic(const wchar_t* text, int x, int y, int w, int h) {
    HWND hw = CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT,
                              S(x), S(y), S(w), S(h), g_hMain, nullptr, nullptr, nullptr);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_font, TRUE);
    return hw;
}

static void subBtn(HWND h) {
    WNDPROC old = (WNDPROC)SetWindowLongPtrW(h, GWLP_WNDPROC, (LONG_PTR)BtnSub);
    g_btnOld[h] = old;
    g_btnHover[h] = false;
}

static HWND mkButton(const wchar_t* text, int id, int x, int y, int w, int h,
                     DWORD style = BS_PUSHBUTTON, int page = -1) {
    HWND hw = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
                              S(x), S(y), S(w), S(h), g_hMain, (HMENU)(INT_PTR)id,
                              nullptr, nullptr);
    SendMessageW(hw, WM_SETFONT, (WPARAM)g_font, TRUE);
    if (style & BS_OWNERDRAW) subBtn(hw);
    if (page >= 0) g_pageCtl[page].push_back(hw);
    return hw;
}

static void buildUi() {
    const int W = 640;

    // 预览卡片
    g_hPreview = CreateWindowExW(0, L"NAPPreview", L"", WS_CHILD | WS_VISIBLE,
                                 S(20), S(64), S(600), S(284),
                                 g_hMain, (HMENU)(INT_PTR)ID_PREVIEW, nullptr, nullptr);

    // 工具行
    const wchar_t* tools[4] = { T(S_TOOL_BRUSH), T(S_TOOL_RECT), T(S_TOOL_FILL), T(S_TOOL_ERASER) };
    for (int i = 0; i < 4; ++i)
        g_hToolBtns[i] = mkButton(tools[i], ID_TOOL_BRUSH + i, 16 + i * 58, 366, 52, 28, BS_OWNERDRAW);

    g_hInvert = mkCheck(g_hMain, ID_INVERT, T(S_INVERT), false, 252, 367, 118, 26);
    mkStatic(T(S_BRUSH), 380, 372, 40, 18);
    g_hBrushSz = mkSlider(g_hMain, ID_BRUSH_SZ, 418, 362, 200, 34, 1, 40, 1.0f, (float)g_brushCells, 1);

    // 编辑按钮行
    mkButton(T(S_BTN_CLEAR), ID_CLEAR, 16, 402, 96, 28);
    mkButton(T(S_BTN_SAVE), ID_SAVE, 120, 402, 96, 28);
    mkButton(T(S_BTN_LOAD), ID_LOAD, 224, 402, 96, 28);
    mkButton(T(S_BTN_RESET), ID_RESET, 328, 402, 130, 28);

    // 标签（pill）
    const int tabs[4] = { S_TAB_LOOK, S_TAB_INNER, S_TAB_NOISE, S_TAB_DISPLAY };
    for (int i = 0; i < 4; ++i)
        g_hTabBtns[i] = mkButton(T(tabs[i]), ID_TAB0 + i, 16 + i * 100, 438, 94, 30, BS_OWNERDRAW);

    // 页面内容
    int rowOfPage[4] = { 0, 0, 0, 0 };
    const int pageLeft = 26, pageTop = 484, rowH = 28;
    for (int id = 0; id < SL_COUNT; ++id) {
        const SDef& d = kSDefs[id];
        int page = d.page;
        int row = rowOfPage[page]++;
        int y = pageTop + row * rowH;
        HWND lbl = mkStatic(T(d.nameId), pageLeft, y + 4, 108, 20);
        g_sliderLbl[id] = lbl;
        g_pageCtl[page].push_back(lbl);
        g_hSliders[id] = mkSlider(g_hMain, id, pageLeft + 110, y, 480, rowH, d.mn, d.mx, d.step,
                                  paramValue(id), d.fmt);
        g_pageCtl[page].push_back(g_hSliders[id]);
    }

    // 外观页：颜色
    {
        const int cnames[4] = { S_COLOR_EDGE, S_COLOR_FILL, S_COLOR_GLOW, S_COLOR_SPARK };
        int y = pageTop + 4 * rowH + 6;
        HWND lbl = mkStatic(T(S_COLOR), pageLeft, y + 6, 80, 20);
        g_pageCtl[0].push_back(lbl);
        for (int i = 0; i < 4; ++i)
            g_hColorBtn[i] = mkButton(T(cnames[i]), ID_COLOR_EDGE + i,
                                      pageLeft + 110 + i * 118, y, 110, 30, BS_OWNERDRAW, 0);
    }

    // 噪域页：说明
    {
        HWND note = mkStatic(T(S_HINT_NOISE), pageLeft, pageTop + 6 * rowH + 6, 570, 40);
        g_pageCtl[2].push_back(note);
    }

    // 显示页
    {
        int y = pageTop + rowH + 6;
        g_hBtnScreen = mkButton(L"", ID_BTN_SCREEN, pageLeft, y, 236, 30, BS_OWNERDRAW, 3);
        g_hBtnTheme = mkButton(L"", ID_BTN_THEME, pageLeft + 248, y, 190, 30, BS_OWNERDRAW, 3);
        y += 40;
        g_hChk[0] = mkCheck(g_hMain, ID_CHK_ENABLED, T(S_CHK_SHOW), true, pageLeft, y, 118, 26);
        g_hChk[1] = mkCheck(g_hMain, ID_CHK_TOPMOST, T(S_CHK_TOPMOST), true, pageLeft + 136, y, 130, 26);
        g_hChk[2] = mkCheck(g_hMain, ID_CHK_PLAYING, T(S_CHK_PLAY), true, pageLeft + 288, y, 110, 26);
        for (int i = 0; i < 3; ++i) g_pageCtl[3].push_back(g_hChk[i]);
        y += 36;
        HWND h1 = mkStatic(T(S_HINT_DISP1), pageLeft, y, 580, 20);
        HWND h2 = mkStatic(T(S_HINT_DISP2), pageLeft, y + 22, 580, 20);
        g_pageCtl[3].push_back(h1);
        g_pageCtl[3].push_back(h2);
    }
}

static void switchTab(int page) {
    g_tabPage = page;
    for (int p = 0; p < 4; ++p) {
        for (HWND h : g_pageCtl[p])
            if (h) ShowWindow(h, p == page ? SW_SHOW : SW_HIDE);
        if (g_hTabBtns[p]) InvalidateRect(g_hTabBtns[p], nullptr, TRUE);
    }
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

// ===========================================================================
// 命令
// ===========================================================================
static void updateScreenButton() {
    wchar_t buf[96];
    swprintf_s(buf, T(S_SCREEN_FMT), T(S_MODE_LEFT + (int)g_app.screenMode));
    SetWindowTextW(g_hBtnScreen, buf);
    InvalidateRect(g_hBtnScreen, nullptr, FALSE);
}

static void updateThemeButton() {
    wchar_t buf[96];
    swprintf_s(buf, T(S_THEME_FMT),
               g_app.theme == ThemeKind::Light ? T(S_THEME_LIGHT) : T(S_THEME_DARK));
    SetWindowTextW(g_hBtnTheme, buf);
    InvalidateRect(g_hBtnTheme, nullptr, FALSE);
}

static void applyThemeAndRedraw(ThemeKind k) {
    g_app.theme = k;
    applyTheme(k);
    updateThemeButton();
    g_app.dirty = true;
    RedrawWindow(g_hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

static void setScreenMode(ScreenMode m) {
    g_app.screenMode = m;
    updateScreenButton();
    refreshTargets();
    g_app.dirty = true;
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

static void resetAll() {
    if (MessageBoxW(g_hMain, T(S_MSG_RESET_TEXT), T(S_MSG_RESET_TITLE),
                    MB_YESNO | MB_ICONQUESTION) != IDYES)
        return;
    App_ResetDefaults();
    applyTheme(g_app.theme);
    refreshAllSliders();
    checkSet(g_hChk[0], g_app.p.enabled);
    checkSet(g_hChk[1], g_app.p.topmost);
    checkSet(g_hChk[2], g_app.p.playing);
    updateScreenButton();
    updateThemeButton();
    refreshTargets();
    g_app.dirty = true;
    RedrawWindow(g_hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

static void screenMenu(HWND owner) {
    HMENU m = CreatePopupMenu();
    for (int i = 0; i < 4; ++i) {
        UINT f = MF_STRING | ((int)g_app.screenMode == i ? MF_CHECKED : 0);
        AppendMenuW(m, f, IDM_LEFT + i, T(S_MODE_LEFT + i));
    }
    POINT pt;
    GetCursorPos(&pt);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN, pt.x, pt.y, 0, owner, nullptr);
    DestroyMenu(m);
}

static void ApplyLanguage();
static HMENU buildMenu();

// ---------------------------------------------------------------------------
static LRESULT CALLBACK MainProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        g_hMain = hwnd;
        buildUi();
        switchTab(0);
        refreshAllSliders();
        checkSet(g_hChk[0], g_app.p.enabled);
        checkSet(g_hChk[1], g_app.p.topmost);
        checkSet(g_hChk[2], g_app.p.playing);
        updateScreenButton();
        updateThemeButton();
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
    case WM_TIMER:
        if (wp == 1) {
            tick();
            static int n = 0;
            if ((++n % 30) == 0) refreshTargets();
            if ((n % 60) == 0) Overlay_KeepTopmost();
        }
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
        if (id >= ID_TAB0 && id <= ID_TAB3) { switchTab(id - ID_TAB0); return 0; }
        if (id >= ID_TOOL_BRUSH && id <= ID_TOOL_ERASER) {
            g_tool = id - ID_TOOL_BRUSH;
            for (int i = 0; i < 4; ++i) InvalidateRect(g_hToolBtns[i], nullptr, FALSE);
            InvalidateRect(g_hPreview, nullptr, FALSE);
            return 0;
        }
        switch (id) {
        case ID_CLEAR: case IDM_CLEAR:
            g_app.grid.clear();
            g_app.dirty = true;
            InvalidateRect(g_hPreview, nullptr, FALSE);
            return 0;
        case ID_SAVE: case IDM_SAVE:
            App_SaveConfig();
            App_SaveMask();
            return 0;
        case ID_LOAD: case IDM_LOAD:
            App_LoadConfig();
            g_lang = g_app.language;
            App_UpdateMaskSpace();
            App_LoadMask();
            refreshAllSliders();
            applyTheme(g_app.theme);
            updateThemeButton();
            updateScreenButton();
            refreshTargets();
            checkSet(g_hChk[0], g_app.p.enabled);
            checkSet(g_hChk[1], g_app.p.topmost);
            checkSet(g_hChk[2], g_app.p.playing);
            RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
            g_app.dirty = true;
            return 0;
        case ID_RESET: case IDM_RESET:
            resetAll();
            return 0;
        case IDM_QUIT:
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            return 0;
        case IDM_DARK:  applyThemeAndRedraw(ThemeKind::Dark);  return 0;
        case IDM_LIGHT: applyThemeAndRedraw(ThemeKind::Light); return 0;
        case IDM_ZH: case IDM_EN:
            g_lang = (id == IDM_EN) ? 1 : 0;
            g_app.language = g_lang;
            ApplyLanguage();
            return 0;
        case IDM_LEFT:  setScreenMode(ScreenMode::Left);  return 0;
        case IDM_RIGHT: setScreenMode(ScreenMode::Right); return 0;
        case IDM_SAME:  setScreenMode(ScreenMode::Same);  return 0;
        case IDM_SPAN:  setScreenMode(ScreenMode::Span);  return 0;
        case IDM_WIZARD:
            showWizard(1);
            return 0;
        case IDM_ABOUT:
            MessageBoxW(hwnd, T(S_MSG_ABOUT_TEXT), T(S_MSG_ABOUT_TITLE), MB_OK | MB_ICONINFORMATION);
            return 0;
        case ID_BTN_SCREEN: screenMenu(hwnd); return 0;
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
        wchar_t text[128]{};
        GetWindowTextW(dis->hwndItem, text, 128);

        const bool isTool = (dis->CtlID >= ID_TOOL_BRUSH && dis->CtlID <= ID_TOOL_ERASER);
        const bool isTab = (dis->CtlID >= ID_TAB0 && dis->CtlID <= ID_TAB3);
        const bool isColor = (dis->CtlID >= ID_COLOR_EDGE && dis->CtlID <= ID_COLOR_SPARK);
        const bool hover = g_btnHover[dis->hwndItem];

        bool selected = false;
        if (isTool) selected = (dis->CtlID - ID_TOOL_BRUSH) == g_tool;
        if (isTab)  selected = (dis->CtlID - ID_TAB0) == g_tabPage;

        RECT r = dis->rcItem;
        int rad = S(isTab ? 15 : 8);
        COLORREF fillc = g_pal.card2;
        if (selected) fillc = g_pal.accent;

        const bool onCard = isColor || dis->CtlID == ID_BTN_SCREEN || dis->CtlID == ID_BTN_THEME;
        HBRUSH bgb = CreateSolidBrush(onCard ? g_pal.card : g_pal.bg);
        FillRect(dis->hDC, &r, bgb);
        DeleteObject(bgb);

        if (isColor) {
            COLORREF c = RGB(120, 120, 130);
            if (dis->CtlID == ID_COLOR_EDGE) c = g_app.p.edgeColor;
            else if (dis->CtlID == ID_COLOR_FILL) c = g_app.p.fillColor;
            else if (dis->CtlID == ID_COLOR_GLOW) c = g_app.p.glowColor;
            else if (dis->CtlID == ID_COLOR_SPARK) c = g_app.p.sparkColor;
            fillRound(dis->hDC, r, rad, c);
            frameRound(dis->hDC, r, rad, g_pal.border);
            RECT tr = r;
            tr.bottom -= S(2);
            drawText2(dis->hDC, text, tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                      RGB(255, 255, 255), g_font);
            return TRUE;
        }

        fillRound(dis->hDC, r, rad, fillc);
        frameRound(dis->hDC, r, rad, selected ? g_pal.accent : g_pal.border);
        COLORREF tc = (selected && isTab) ? RGB(255, 255, 255) : g_pal.text;
        drawText2(dis->hDC, text, r, DT_CENTER | DT_VCENTER | DT_SINGLELINE, tc, g_font);
        return TRUE;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_pal.text);
        SetBkMode(dc, TRANSPARENT);
        return (LRESULT)GetStockObject(HOLLOW_BRUSH);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetTextColor(dc, g_pal.text);
        SetBkColor(dc, g_pal.card);
        static HBRUSH br = nullptr;
        if (br) DeleteObject(br);
        br = CreateSolidBrush(g_pal.card);
        return (LRESULT)br;
    }
    case WM_ERASEBKGND: {
        HDC dc = (HDC)wp;
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_brBg);
        return 1;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, g_brBg);

        // 顶部标题栏
        RECT tr{ S(16), S(10), rc.right - S(16), S(38) };
        drawText2(dc, T(S_APP_TITLE), tr, DT_LEFT | DT_SINGLELINE, g_pal.text, g_fontB);
        RECT vr{ S(16), S(38), rc.right - S(16), S(52) };
        drawText2(dc, L"block-area overlay  ·  v1.1", vr, DT_LEFT | DT_SINGLELINE,
                  g_pal.subtle, g_fontSm);
        RECT bar{ S(16), S(50), S(52), S(53) };
        fillRound(dc, bar, S(3), g_pal.accent);

        // 预览卡片 / 内容卡片
        RECT pc{ S(12), S(58), rc.right - S(12), S(356) };
        fillRound(dc, pc, S(12), g_pal.card);
        frameRound(dc, pc, S(12), g_pal.border);

        RECT cc{ S(12), S(478), rc.right - S(12), S(714) };
        fillRound(dc, cc, S(12), g_pal.card);
        frameRound(dc, cc, S(12), g_pal.border);

        RECT st{ S(16), S(722), rc.right - S(16), S(742) };
        drawText2(dc, T(S_HINT_CANVAS), st, DT_LEFT | DT_SINGLELINE, g_pal.subtle, g_fontSm);
        EndPaint(hwnd, &ps);
        return 0;
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
// 语言切换（就地刷新文案）
// ===========================================================================
static void ApplyLanguage() {
    if (!g_hMain) return;
    SetWindowTextW(g_hMain, T(S_APP_TITLE));
    const wchar_t* tools[4] = { T(S_TOOL_BRUSH), T(S_TOOL_RECT), T(S_TOOL_FILL), T(S_TOOL_ERASER) };
    for (int i = 0; i < 4; ++i) SetWindowTextW(g_hToolBtns[i], tools[i]);
    const int tabs[4] = { S_TAB_LOOK, S_TAB_INNER, S_TAB_NOISE, S_TAB_DISPLAY };
    for (int i = 0; i < 4; ++i) SetWindowTextW(g_hTabBtns[i], T(tabs[i]));

    SetWindowTextW(g_hInvert, T(S_INVERT));
    SetWindowTextW((HWND)GetDlgItem(g_hMain, ID_CLEAR), T(S_BTN_CLEAR));
    SetWindowTextW((HWND)GetDlgItem(g_hMain, ID_SAVE), T(S_BTN_SAVE));
    SetWindowTextW((HWND)GetDlgItem(g_hMain, ID_LOAD), T(S_BTN_LOAD));
    SetWindowTextW((HWND)GetDlgItem(g_hMain, ID_RESET), T(S_BTN_RESET));
    const int cn[4] = { S_COLOR_EDGE, S_COLOR_FILL, S_COLOR_GLOW, S_COLOR_SPARK };
    for (int i = 0; i < 4; ++i) SetWindowTextW(g_hColorBtn[i], T(cn[i]));

    for (int i = 0; i < SL_COUNT; ++i) {
        if (g_sliderLbl[i]) SetWindowTextW(g_sliderLbl[i], T(kSDefs[i].nameId));
        if (g_hSliders[i]) InvalidateRect(g_hSliders[i], nullptr, FALSE);
    }
    checkSet(g_hChk[0], g_app.p.enabled);
    SetWindowTextW(g_hChk[0], T(S_CHK_SHOW));
    SetWindowTextW(g_hChk[1], T(S_CHK_TOPMOST));
    SetWindowTextW(g_hChk[2], T(S_CHK_PLAY));

    updateScreenButton();
    updateThemeButton();

    // 重建菜单
    HMENU m = buildMenu();
    SetMenu(g_hMain, m);
    DrawMenuBar(g_hMain);

    g_app.dirty = true;
    RedrawWindow(g_hMain, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    InvalidateRect(g_hPreview, nullptr, FALSE);
}

// ===========================================================================
// 菜单
// ===========================================================================
static HMENU buildMenu() {
    HMENU mb = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_SAVE, T(S_MENU_SAVE));
    AppendMenuW(file, MF_STRING, IDM_LOAD, T(S_MENU_LOAD));
    AppendMenuW(file, MF_STRING, IDM_CLEAR, T(S_MENU_CLEAR));
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_RESET, T(S_MENU_RESET));
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_QUIT, T(S_MENU_QUIT));

    HMENU view = CreatePopupMenu();
    AppendMenuW(view, MF_STRING, IDM_DARK, T(S_MENU_DARK));
    AppendMenuW(view, MF_STRING, IDM_LIGHT, T(S_MENU_LIGHT));
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    for (int i = 0; i < 4; ++i)
        AppendMenuW(view, MF_STRING, IDM_LEFT + i, T(S_MENU_LEFT + i));

    HMENU langM = CreatePopupMenu();
    AppendMenuW(langM, MF_STRING | (g_lang == 0 ? MF_CHECKED : 0), IDM_ZH, L"简体中文");
    AppendMenuW(langM, MF_STRING | (g_lang == 1 ? MF_CHECKED : 0), IDM_EN, L"English");
    AppendMenuW(view, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(view, MF_POPUP, (UINT_PTR)langM, g_lang ? L"Language" : L"语言");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, IDM_WIZARD, T(S_MENU_WIZARD));
    AppendMenuW(help, MF_STRING, IDM_ABOUT, T(S_MENU_ABOUT));

    AppendMenuW(mb, MF_POPUP, (UINT_PTR)file, T(S_MENU_FILE));
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)view, T(S_MENU_VIEW));
    AppendMenuW(mb, MF_POPUP, (UINT_PTR)help, T(S_MENU_HELP));
    return mb;
}

// ===========================================================================
// 对外
// ===========================================================================
bool Control_Init(HINSTANCE inst) {
    int dpi = 96;
    {
        HDC dc = GetDC(nullptr);
        if (dc) { dpi = GetDeviceCaps(dc, LOGPIXELSX); ReleaseDC(nullptr, dc); }
    }
    g_dpi = dpi;
    g_lang = g_app.language;

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    applyTheme(g_app.theme);
    g_font = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                         CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_fontB = CreateFontW(-MulDiv(12, dpi, 72), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei UI");
    g_fontSm = CreateFontW(-MulDiv(8, dpi, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
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
    reg(MainProc, L"NoiseAreaMain", 32512, g_brBg);
    reg(PreviewProc, L"NAPPreview", 32515, g_brBg);
    reg(SliderProc, L"NASlider", 32512, nullptr);
    reg(CheckProc, L"NACheck", 32512, nullptr);
    reg(WizardProc, L"NAWizard", 32512, g_brBg);

    const int clientW = S(640), clientH = S(752);
    RECT rc{ 0, 0, clientW, clientH };
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, TRUE, WS_EX_TOPMOST);
    const int winW = rc.right - rc.left, winH = rc.bottom - rc.top;
    const int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    int px = (sw - winW) / 2, py = (sh - winH) / 3;
    if (px < 0) px = 0;
    if (py < 0) py = 0;

    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_APPWINDOW | WS_CLIPCHILDREN,
        L"NoiseAreaMain", T(S_APP_TITLE),
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        px, py, winW, winH, nullptr, buildMenu(), inst, nullptr);
    if (!hwnd) return false;
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    return true;
}

HWND Control_GetHwnd() { return g_hMain; }

void Control_ShowWizard() { showWizard(g_app.firstLaunch ? 0 : 1); }

void Control_RefreshTargets() { refreshTargets(); }
