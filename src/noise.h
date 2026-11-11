// NoiseArea_forwin - 噪域（block area）渲染核心
// 独立实现的 block-area 遮罩与环带管线：格子遮罩 → 环带提取 → 分层着色。
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdint>
#include <vector>

// 编辑网格（坐标 = 遮罩空间，左上角 0,0）
//   0      = 空
//   bit0   = 普通噪域区
//   bit1   = 反相 / 减色区（官方 disabled 材质）
struct Grid {
    int gw = 0, gh = 0;
    int blockPx = 8;
    int vx = 0, vy = 0, vw = 0, vh = 0;   // 遮罩空间 (vx/vy 恒为 0)
    std::vector<uint8_t> cells;

    void ensure(int vx_, int vy_, int vw_, int vh_, int blockPx_);
    void clear();
    bool empty() const {
        for (uint8_t v : cells) if (v) return false;
        return true;
    }
    bool inBounds(int cx, int cy) const { return cx >= 0 && cy >= 0 && cx < gw && cy < gh; }
    uint8_t& at(int cx, int cy) { return cells[(size_t)cy * gw + cx]; }
    uint8_t at(int cx, int cy) const { return cells[(size_t)cy * gw + cx]; }

    void screenToCell(int sx, int sy, int& cx, int& cy) const {
        cx = (sx - vx) / blockPx;
        cy = (sy - vy) / blockPx;
    }
    void stampCircle(float sx, float sy, float radiusPx, int value);
    void strokeLine(float x0, float y0, float x1, float y1, float radiusPx, int value);
    void applyRect(int cx0, int cy0, int cx1, int cy1, int value);
    int  floodFill(int cx, int cy, int value);
};

// 参数：名称与默认值对齐网页（BASE_FLOATS / BASE_COLORS / SLIDERS）
struct Params {
    // 外观
    float edgeOpacity     = 0.80f;   // _EdgeOpacity            0..2
    float glowIntensity   = 0.80f;   // _GlowIntensity          0..3
    int   edgeStrength    = 6;       // 边缘环带宽度 1..6（越大越细）
    int   blockPx         = 8;       // 一格多少像素（官方 1/8 分辨率 => 8）
    // 内部
    float fillOpacity     = 0.667f;  // _FillOpacity            0..1.5
    float displaceStrength= 0.15f;   // _DisplaceStrength       0..0.5
    float displaceBlend   = 0.411f;  // _DisplaceBlendIntensity 0..1
    float displaceSpeed   = 1.5f;    // _DisplaceSpeed          0..5
    float sparkHueShift   = 0.20f;   // _SparkHueShiftAmount    0..1
    float sparkOpacity    = 5.69f;   // _SparkMapOpacity        0..12
    float shineSpeed      = 37.9f;   // _ShineSpeed             0..60
    float shineBrightness = 0.12f;   // _ShineBrightness        0..0.5
    // 噪域（核心）
    float noiseStrength   = 1.0f;    // _NoiseDisplaceStrength  0..2
    float noiseRadius     = 0.48f;   // _NoiseRadius            0.1..1.5
    float noiseSmoothness = 1.0f;    // _NoiseSmoothness        0..2
    float noiseEvoSpeed   = 0.03f;   // _NoiseEvoSpeed          0..0.2
    float noiseDirChange  = 60.0f;   // _NoiseDirChangeSpeed    0..120
    float edgeJitter      = 1.00f;   // 边缘抖动：位移振幅倍率 0..2（1 = 原振幅 0.1）
    // 动画
    float animSpeed       = 1.0f;

    bool  playing  = true;
    bool  enabled  = true;
    bool  topmost  = true;

    COLORREF edgeColor = RGB(255,  84,  84);
    COLORREF fillColor = RGB(182,  60,  60);
    COLORREF glowColor = RGB(255,  46,  46);
    COLORREF sparkColor= RGB(255,  73,  73);
};

// 合成后的格子颜色（BGRA，预乘 alpha，可直接给 UpdateLayeredWindow）
void buildCellColors(const Grid& g, const Params& p, double timeSec,
                     std::vector<uint32_t>& outCells);

// 最近邻展开：把格子图放大成 w*h 的 BGRA 像素（stridePx 单位为像素）
void expandCells(const Grid& g, const std::vector<uint32_t>& cells,
                 uint32_t* dst, int w, int h, int stridePx);
