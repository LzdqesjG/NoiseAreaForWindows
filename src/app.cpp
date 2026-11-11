#include "app.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>

AppState g_app;

// ---------------------------------------------------------------------------
// 路径
// ---------------------------------------------------------------------------
std::wstring App_ExeDir() {
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring s(exe);
    size_t p = s.find_last_of(L"\\/");
    if (p != std::wstring::npos) s = s.substr(0, p + 1);
    return s;
}
std::wstring App_ConfigPath() { return App_ExeDir() + L"config.json"; }
std::wstring App_MaskPath()   { return App_ExeDir() + L"mask.dat"; }

const wchar_t* ScreenModeName(ScreenMode m) {
    switch (m) {
    case ScreenMode::Left:  return L"仅左屏";
    case ScreenMode::Right: return L"仅右屏";
    case ScreenMode::Same:  return L"左右同内容";
    default:                return L"左右跨区";
    }
}

// ---------------------------------------------------------------------------
// 极简 JSON 读取（只认我们自己的扁平 key）
// ---------------------------------------------------------------------------
static const char* findKey(const std::string& js, const char* key) {
    std::string pat = std::string("\"") + key + "\"";
    size_t p = js.find(pat);
    if (p == std::string::npos) return nullptr;
    p += pat.size();
    while (p < js.size() && (js[p] == ' ' || js[p] == '\t' || js[p] == '\r' || js[p] == '\n')) ++p;
    if (p >= js.size() || js[p] != ':') return nullptr;
    ++p;
    while (p < js.size() && (js[p] == ' ' || js[p] == '\t' || js[p] == '\r' || js[p] == '\n')) ++p;
    return js.c_str() + p;
}
static float jsonNum(const std::string& js, const char* k, float def) {
    const char* v = findKey(js, k);
    return v ? (float)atof(v) : def;
}
static int jsonInt(const std::string& js, const char* k, int def) {
    const char* v = findKey(js, k);
    return v ? atoi(v) : def;
}
static bool jsonBool(const std::string& js, const char* k, bool def) {
    const char* v = findKey(js, k);
    if (!v) return def;
    return strncmp(v, "true", 4) == 0;
}
static std::string jsonStr(const std::string& js, const char* k, const char* def) {
    const char* v = findKey(js, k);
    if (!v || *v != '"') return def;
    ++v;
    const char* e = strchr(v, '"');
    if (!e) return def;
    return std::string(v, (size_t)(e - v));
}
static COLORREF jsonColor(const std::string& js, const char* k, COLORREF def) {
    std::string s = jsonStr(js, k, "");
    if (s.size() != 7 || s[0] != '#') return def;
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    int r = hex(s[1]) * 16 + hex(s[2]);
    int g = hex(s[3]) * 16 + hex(s[4]);
    int b = hex(s[5]) * 16 + hex(s[6]);
    return RGB(r, g, b);
}

// ---------------------------------------------------------------------------
// 配置读写
// ---------------------------------------------------------------------------
static const char* modeKey(ScreenMode m) {
    switch (m) {
    case ScreenMode::Left:  return "left";
    case ScreenMode::Right: return "right";
    case ScreenMode::Same:  return "same";
    default:                return "span";
    }
}

void App_LoadConfig() {
    std::ifstream f(App_ConfigPath().c_str(), std::ios::binary);
    if (!f) {
        g_app.firstLaunch = true;
        return;
    }
    std::string js((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (js.find("\"version\"") == std::string::npos) {
        g_app.firstLaunch = true;
        return;
    }
    g_app.firstLaunch = jsonBool(js, "firstLaunch", true);

    std::string theme = jsonStr(js, "theme", "dark");
    g_app.theme = (theme == "light") ? ThemeKind::Light : ThemeKind::Dark;

    g_app.language = (jsonStr(js, "language", "zh") == "en") ? 1 : 0;

    std::string mode = jsonStr(js, "screenMode", "span");
    if (mode == "left") g_app.screenMode = ScreenMode::Left;
    else if (mode == "right") g_app.screenMode = ScreenMode::Right;
    else if (mode == "same") g_app.screenMode = ScreenMode::Same;
    else g_app.screenMode = ScreenMode::Span;

    auto& p = g_app.p;
    p.enabled  = jsonBool(js, "enabled", p.enabled);
    p.topmost  = jsonBool(js, "topmost", p.topmost);
    p.playing  = jsonBool(js, "playing", p.playing);

    p.edgeOpacity      = jsonNum(js, "edgeOpacity", p.edgeOpacity);
    p.glowIntensity    = jsonNum(js, "glowIntensity", p.glowIntensity);
    p.edgeStrength     = jsonInt(js, "edgeStrength", p.edgeStrength);
    p.blockPx          = jsonInt(js, "blockPx", p.blockPx);
    p.fillOpacity      = jsonNum(js, "fillOpacity", p.fillOpacity);
    p.displaceStrength = jsonNum(js, "displaceStrength", p.displaceStrength);
    p.displaceBlend    = jsonNum(js, "displaceBlend", p.displaceBlend);
    p.displaceSpeed    = jsonNum(js, "displaceSpeed", p.displaceSpeed);
    p.sparkHueShift    = jsonNum(js, "sparkHueShift", p.sparkHueShift);
    p.sparkOpacity     = jsonNum(js, "sparkOpacity", p.sparkOpacity);
    p.shineSpeed       = jsonNum(js, "shineSpeed", p.shineSpeed);
    p.shineBrightness  = jsonNum(js, "shineBrightness", p.shineBrightness);
    p.noiseStrength    = jsonNum(js, "noiseStrength", p.noiseStrength);
    p.noiseRadius      = jsonNum(js, "noiseRadius", p.noiseRadius);
    p.noiseSmoothness  = jsonNum(js, "noiseSmoothness", p.noiseSmoothness);
    p.noiseEvoSpeed    = jsonNum(js, "noiseEvoSpeed", p.noiseEvoSpeed);
    p.noiseDirChange   = jsonNum(js, "noiseDirChange", p.noiseDirChange);
    p.edgeJitter       = jsonNum(js, "edgeJitter", p.edgeJitter);
    p.animSpeed        = jsonNum(js, "animSpeed", p.animSpeed);

    p.edgeColor  = jsonColor(js, "edge",  p.edgeColor);
    p.fillColor  = jsonColor(js, "fill",  p.fillColor);
    p.glowColor  = jsonColor(js, "glow",  p.glowColor);
    p.sparkColor = jsonColor(js, "spark", p.sparkColor);

    if (p.blockPx < 2) p.blockPx = 2;
    if (p.edgeStrength < 1) p.edgeStrength = 1;
    if (p.edgeStrength > 6) p.edgeStrength = 6;
}

void App_SaveConfig() {
    const auto& p = g_app.p;
    std::ofstream f(App_ConfigPath().c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return;
    f.setf(std::ios::fixed);
    f.precision(4);
    f << "{\n";
    f << "  \"version\": 2,\n";
    f << "  \"firstLaunch\": " << (g_app.firstLaunch ? "true" : "false") << ",\n";
    f << "  \"theme\": \"" << (g_app.theme == ThemeKind::Light ? "light" : "dark") << "\",\n";
    f << "  \"language\": \"" << (g_app.language ? "en" : "zh") << "\",\n";
    f << "  \"screenMode\": \"" << modeKey(g_app.screenMode) << "\",\n";
    f << "  \"enabled\": " << (p.enabled ? "true" : "false") << ",\n";
    f << "  \"topmost\": " << (p.topmost ? "true" : "false") << ",\n";
    f << "  \"playing\": " << (p.playing ? "true" : "false") << ",\n";
    f << "  \"blockPx\": " << p.blockPx << ",\n";
    f << "  \"edgeStrength\": " << p.edgeStrength << ",\n";
    f << "  \"edgeOpacity\": " << p.edgeOpacity << ",\n";
    f << "  \"glowIntensity\": " << p.glowIntensity << ",\n";
    f << "  \"fillOpacity\": " << p.fillOpacity << ",\n";
    f << "  \"displaceStrength\": " << p.displaceStrength << ",\n";
    f << "  \"displaceBlend\": " << p.displaceBlend << ",\n";
    f << "  \"displaceSpeed\": " << p.displaceSpeed << ",\n";
    f << "  \"sparkHueShift\": " << p.sparkHueShift << ",\n";
    f << "  \"sparkOpacity\": " << p.sparkOpacity << ",\n";
    f << "  \"shineSpeed\": " << p.shineSpeed << ",\n";
    f << "  \"shineBrightness\": " << p.shineBrightness << ",\n";
    f << "  \"noiseStrength\": " << p.noiseStrength << ",\n";
    f << "  \"noiseRadius\": " << p.noiseRadius << ",\n";
    f << "  \"noiseSmoothness\": " << p.noiseSmoothness << ",\n";
    f << "  \"noiseEvoSpeed\": " << p.noiseEvoSpeed << ",\n";
    f << "  \"noiseDirChange\": " << p.noiseDirChange << ",\n";
    f << "  \"edgeJitter\": " << p.edgeJitter << ",\n";
    f << "  \"animSpeed\": " << p.animSpeed << ",\n";
    char buf[32];
    auto hex = [&](COLORREF c) {
        sprintf_s(buf, "#%02X%02X%02X", GetRValue(c), GetGValue(c), GetBValue(c));
        return std::string(buf);
    };
    f << "  \"edge\": \"" << hex(p.edgeColor) << "\",\n";
    f << "  \"fill\": \"" << hex(p.fillColor) << "\",\n";
    f << "  \"glow\": \"" << hex(p.glowColor) << "\",\n";
    f << "  \"spark\": \"" << hex(p.sparkColor) << "\"\n";
    f << "}\n";
}

void App_SaveMask() {
    std::ofstream f(App_MaskPath().c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return;
    const uint32_t magic = 0x314D414Eu;   // "NAM1"
    int32_t gw = g_app.grid.gw, gh = g_app.grid.gh, bp = g_app.grid.blockPx;
    f.write((const char*)&magic, 4);
    f.write((const char*)&gw, 4);
    f.write((const char*)&gh, 4);
    f.write((const char*)&bp, 4);
    if (g_app.grid.cells.size() == (size_t)gw * gh)
        f.write((const char*)g_app.grid.cells.data(), (std::streamsize)g_app.grid.cells.size());
}

void App_LoadMask() {
    std::ifstream f(App_MaskPath().c_str(), std::ios::binary);
    if (!f) return;
    uint32_t magic = 0;
    int32_t gw = 0, gh = 0, bp = 0;
    f.read((char*)&magic, 4);
    if (magic != 0x314D414Eu) return;
    f.read((char*)&gw, 4);
    f.read((char*)&gh, 4);
    f.read((char*)&bp, 4);
    if (gw <= 0 || gh <= 0 || gw > 8192 || gh > 8192) return;

    std::vector<uint8_t> old((size_t)gw * gh, 0);
    f.read((char*)old.data(), (std::streamsize)old.size());
    if (!f) return;

    App_UpdateMaskSpace();
    auto& gr = g_app.grid;
    for (int y = 0; y < gr.gh; ++y)
        for (int x = 0; x < gr.gw; ++x) {
            int ox = (int)((x + 0.5f) * gw / gr.gw);
            int oy = (int)((y + 0.5f) * gh / gr.gh);
            if (ox >= gw) ox = gw - 1;
            if (oy >= gh) oy = gh - 1;
            gr.cells[(size_t)y * gr.gw + x] = old[(size_t)oy * gw + ox];
        }
}

void App_ResetDefaults() {
    // 运行时删除 config.json，随后立刻补一份默认（firstLaunch=false），所以不会再触发教程
    DeleteFileW(App_ConfigPath().c_str());
    g_app.p = Params();
    g_app.screenMode = ScreenMode::Span;
    g_app.theme = ThemeKind::Dark;
    g_app.timeSec = 6.0;
    g_app.firstLaunch = false;
    App_UpdateMaskSpace();
    g_app.grid.clear();
    g_app.dirty = true;
    App_SaveConfig();
    App_SaveMask();
}

// ---------------------------------------------------------------------------
// 显示器
// ---------------------------------------------------------------------------
static BOOL CALLBACK monEnum(HMONITOR, HDC, LPRECT r, LPARAM lp) {
    ((std::vector<RECT>*)lp)->push_back(*r);
    return TRUE;
}

std::vector<RECT> App_GetMonitors() {
    std::vector<RECT> v;
    EnumDisplayMonitors(nullptr, nullptr, monEnum, (LPARAM)&v);
    if (v.empty()) {
        RECT r{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        if (r.right <= 0) r.right = 1920;
        if (r.bottom <= 0) r.bottom = 1080;
        v.push_back(r);
    }
    std::sort(v.begin(), v.end(), [](const RECT& a, const RECT& b) { return a.left < b.left; });
    return v;
}

void App_UpdateMaskSpace() {
    auto mons = App_GetMonitors();
    int w = 1920, h = 1080;
    switch (g_app.screenMode) {
    case ScreenMode::Left:
    case ScreenMode::Same: {
        const RECT& r = mons.front();
        w = r.right - r.left; h = r.bottom - r.top;
        break;
    }
    case ScreenMode::Right: {
        const RECT& r = mons.back();
        w = r.right - r.left; h = r.bottom - r.top;
        break;
    }
    case ScreenMode::Span:
    default: {
        RECT u = mons.front();
        for (const RECT& r : mons) {
            u.left = std::min(u.left, r.left);
            u.top = std::min(u.top, r.top);
            u.right = std::max(u.right, r.right);
            u.bottom = std::max(u.bottom, r.bottom);
        }
        w = u.right - u.left; h = u.bottom - u.top;
        break;
    }
    }
    if (w <= 0) w = 1920;
    if (h <= 0) h = 1080;
    g_app.grid.ensure(0, 0, w, h, g_app.p.blockPx);
}

std::vector<OverlayTarget> App_OverlayTargets() {
    std::vector<OverlayTarget> out;
    auto mons = App_GetMonitors();
    switch (g_app.screenMode) {
    case ScreenMode::Left: {
        const RECT& r = mons.front();
        out.push_back({ r.left, r.top, r.right - r.left, r.bottom - r.top });
        break;
    }
    case ScreenMode::Right: {
        const RECT& r = mons.back();
        out.push_back({ r.left, r.top, r.right - r.left, r.bottom - r.top });
        break;
    }
    case ScreenMode::Same: {
        for (const RECT& r : mons)
            out.push_back({ r.left, r.top, r.right - r.left, r.bottom - r.top });
        break;
    }
    case ScreenMode::Span:
    default: {
        RECT u = mons.front();
        for (const RECT& r : mons) {
            u.left = std::min(u.left, r.left);
            u.top = std::min(u.top, r.top);
            u.right = std::max(u.right, r.right);
            u.bottom = std::max(u.bottom, r.bottom);
        }
        out.push_back({ u.left, u.top, u.right - u.left, u.bottom - u.top });
        break;
    }
    }
    return out;
}
