#include "noise.h"
#include <cmath>
#include <cstring>
#include <algorithm>

// ---------------------------------------------------------------------------
// 基础工具
// ---------------------------------------------------------------------------
static inline uint32_t ihash(uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    x ^= x >> 16; return x;
}
static inline float hashf(int x, int y, int s) {
    uint32_t h = ihash((uint32_t)x * 0x27d4eb2du ^ (uint32_t)y * 0x165667b1u ^ (uint32_t)s * 0x9e3779b9u);
    return (float)(h >> 8) * (1.0f / 16777216.0f);
}
static inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
static inline float mixf(float a, float b, float t) { return a + (b - a) * t; }

// 值噪声（双线性 + smoothstep）
static inline float vnoise(float x, float y, int seed) {
    float fx = std::floor(x), fy = std::floor(y);
    int x0 = (int)fx, y0 = (int)fy;
    float tx = x - fx, ty = y - fy;
    float ux = tx * tx * (3.0f - 2.0f * tx);
    float uy = ty * ty * (3.0f - 2.0f * ty);
    float a = hashf(x0, y0, seed),     b = hashf(x0 + 1, y0, seed);
    float c = hashf(x0, y0 + 1, seed), d = hashf(x0 + 1, y0 + 1, seed);
    return mixf(mixf(a, b, ux), mixf(c, d, ux), uy);
}
static float fbm(float x, float y, int oct, int seed) {
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int i = 0; i < oct; ++i) {
        sum += amp * vnoise(x, y, seed + i * 17);
        norm += amp;
        x *= 2.03f; y *= 2.03f; amp *= 0.5f;
    }
    return sum / norm;
}

static inline uint8_t clampByte(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return (uint8_t)(v * 255.0f + 0.5f);
}

// ---------------------------------------------------------------------------
// Grid
// ---------------------------------------------------------------------------
void Grid::ensure(int vx_, int vy_, int vw_, int vh_, int blockPx_) {
    blockPx_ = std::max(1, blockPx_);
    int ngw = std::max(1, (vw_ + blockPx_ - 1) / blockPx_);
    int ngh = std::max(1, (vh_ + blockPx_ - 1) / blockPx_);
    vx = vx_; vy = vy_; vw = vw_; vh = vh_;
    if (ngw == gw && ngh == gh && blockPx_ == blockPx) return;

    std::vector<uint8_t> old;
    int ogw = gw, ogh = gh, obp = blockPx;
    const bool blockChanged = (blockPx_ != blockPx);
    const bool sizeChanged = (ngw != gw || ngh != gh);
    if (!cells.empty()) old.swap(cells);
    gw = ngw; gh = ngh; blockPx = blockPx_;
    cells.assign((size_t)gw * gh, 0);
    if (old.empty()) return;

    if (blockChanged && !sizeChanged) {
        // 只换格子大小：按位置重采样，覆盖区域不跑位
        for (int y = 0; y < gh; ++y) {
            for (int x = 0; x < gw; ++x) {
                int ox = (int)(((x + 0.5f) * blockPx) / obp);
                int oy = (int)(((y + 0.5f) * blockPx) / obp);
                if (ox < 0 || oy < 0 || ox >= ogw || oy >= ogh) continue;
                cells[(size_t)y * gw + x] = old[(size_t)oy * ogw + ox];
            }
        }
    } else {
        // 空间尺寸也变了（换显示器模式等）：按比例重采样
        for (int y = 0; y < gh; ++y) {
            for (int x = 0; x < gw; ++x) {
                int ox = (int)((x + 0.5f) * ogw / gw);
                int oy = (int)((y + 0.5f) * ogh / gh);
                ox = std::min(ox, ogw - 1);
                oy = std::min(oy, ogh - 1);
                cells[(size_t)y * gw + x] = old[(size_t)oy * ogw + ox];
            }
        }
    }
}

void Grid::clear() { std::fill(cells.begin(), cells.end(), (uint8_t)0); }

// 与网页 paintCell 的位语义一致：0 清空，1 置普通位，2 置反相位
static inline void paintCellValue(uint8_t& cur, int value) {
    if (value == 0) cur = 0;
    else if (value == 2) cur = (uint8_t)((cur | 2) & ~1);
    else cur = (uint8_t)((cur & ~2) | 1);
}

void Grid::stampCircle(float sx, float sy, float radiusPx, int value) {
    if (cells.empty()) return;
    float bx = (sx - vx) / blockPx, by = (sy - vy) / blockPx;
    float r = std::max(0.5f, radiusPx / blockPx);
    int cx0 = (int)std::floor(bx - r), cx1 = (int)std::floor(bx + r);
    int cy0 = (int)std::floor(by - r), cy1 = (int)std::floor(by + r);
    float rr = r * r;
    for (int cy = cy0; cy <= cy1; ++cy) {
        if (cy < 0 || cy >= gh) continue;
        for (int cx = cx0; cx <= cx1; ++cx) {
            if (cx < 0 || cx >= gw) continue;
            float dx = (cx + 0.5f) - bx, dy = (cy + 0.5f) - by;
            if (dx * dx + dy * dy > rr) continue;
            paintCellValue(at(cx, cy), value);
        }
    }
}

void Grid::strokeLine(float x0, float y0, float x1, float y1, float radiusPx, int value) {
    float dx = x1 - x0, dy = y1 - y0;
    float dist = std::sqrt(dx * dx + dy * dy);
    float stepPx = std::max(1.0f, radiusPx * 0.35f);
    int steps = std::max(1, (int)std::ceil(dist / stepPx));
    for (int i = 0; i <= steps; ++i) {
        float k = (float)i / steps;
        stampCircle(x0 + dx * k, y0 + dy * k, radiusPx, value);
    }
}

void Grid::applyRect(int cx0, int cy0, int cx1, int cy1, int value) {
    if (cells.empty()) return;
    if (cx0 > cx1) std::swap(cx0, cx1);
    if (cy0 > cy1) std::swap(cy0, cy1);
    cx0 = std::max(0, cx0); cy0 = std::max(0, cy0);
    cx1 = std::min(gw - 1, cx1); cy1 = std::min(gh - 1, cy1);
    for (int y = cy0; y <= cy1; ++y)
        for (int x = cx0; x <= cx1; ++x)
            paintCellValue(at(x, y), value);
}

int Grid::floodFill(int cx, int cy, int value) {
    if (!inBounds(cx, cy)) return 0;
    auto key = [&](int x, int y) -> int {
        uint8_t v = at(x, y);
        if (value == 2) return (v >> 1) & 1;
        return v & 1;
    };
    const int k0 = key(cx, cy);
    if (value == 0) { if (at(cx, cy) == 0) return 0; }
    else { if (k0 == 1) return 0; }

    std::vector<uint8_t> seen((size_t)gw * gh, 0);
    std::vector<int> stack;
    stack.push_back(cy * gw + cx);
    seen[(size_t)cy * gw + cx] = 1;
    int filled = 0;
    while (!stack.empty()) {
        int i = stack.back(); stack.pop_back();
        int x = i % gw, y = i / gw;
        paintCellValue(cells[i], value);
        ++filled;
        const int nb[4][2] = { {x - 1,y},{x + 1,y},{x,y - 1},{x,y + 1} };
        for (auto& n : nb) {
            int nx = n[0], ny = n[1];
            if (nx < 0 || ny < 0 || nx >= gw || ny >= gh) continue;
            int ni = ny * gw + nx;
            if (seen[ni]) continue;
            if (key(nx, ny) != k0) continue;
            seen[ni] = 1;
            stack.push_back(ni);
        }
    }
    return filled;
}

// ---------------------------------------------------------------------------
// 环带：官方九宫格膨胀 + 权重（GLOW_W 与网页同式）
// ---------------------------------------------------------------------------
static float sGlowW[6];
static bool  sGlowInit = false;
static void ensureGlowW() {
    if (sGlowInit) return;
    float sum = 0.0f;
    for (int k = 1; k <= 6; ++k) sum += std::pow((float)k, 2.65f);
    for (int p = 0; p < 6; ++p) sGlowW[p] = std::pow((float)(6 - p), 2.65f) / sum;
    sGlowInit = true;
}

static void dilate3x3(const std::vector<uint8_t>& src, std::vector<uint8_t>& dst, int w, int h) {
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            uint8_t v = 0;
            for (int dy = -1; dy <= 1 && !v; ++dy) {
                int yy = y + dy;
                if (yy < 0 || yy >= h) continue;
                const uint8_t* row = &src[(size_t)yy * w];
                for (int dx = -1; dx <= 1; ++dx) {
                    int xx = x + dx;
                    if (xx < 0 || xx >= w) continue;
                    if (row[xx]) { v = 1; break; }
                }
            }
            dst[(size_t)y * w + x] = v;
        }
    }
}

// ---------------------------------------------------------------------------
// 合成
// ---------------------------------------------------------------------------
static inline void colorToF(COLORREF c, float& r, float& g, float& b) {
    r = GetRValue(c) / 255.0f;
    g = GetGValue(c) / 255.0f;
    b = GetBValue(c) / 255.0f;
}

void buildCellColors(const Grid& g, const Params& p, double timeSec,
                     std::vector<uint32_t>& out) {
    const int gw = g.gw, gh = g.gh;
    const size_t n = (size_t)gw * gh;
    out.assign(n, 0u);
    if (n == 0 || g.cells.size() != n) return;
    ensureGlowW();

    std::vector<uint8_t> occ(n, 0);
    for (size_t i = 0; i < n; ++i) occ[i] = g.cells[i] ? 1 : 0;

    std::vector<uint8_t> edgeOf(n, 0);
    std::vector<float>   glowOf(n, 0.0f);

    const int ep = 7 - std::max(1, std::min(6, p.edgeStrength));
    if (ep > 0) {
        std::vector<uint8_t> ping = occ, pong(n, 0);
        for (int pass = 0; pass < 6; ++pass) {
            if (sGlowW[pass] < 0.01f) break;
            dilate3x3(ping, pong, gw, gh);
            const float w = sGlowW[pass];
            for (size_t i = 0; i < n; ++i) {
                if (pong[i] && !ping[i]) {
                    if (pass < ep) edgeOf[i] = 1;
                    glowOf[i] += w;
                }
            }
            ping.swap(pong);
        }
    }

    const float t = (float)timeSec;
    const float sp = clampf(p.noiseStrength, 0.0f, 2.0f);
    // 噪声半径越大，团块越大（空间频率越低）
    const float radius = std::max(0.08f, p.noiseRadius);
    const float freq = 1.6f / (radius * 2.2f);
    const float smoothMix = clampf(p.noiseSmoothness, 0.0f, 2.0f) * 0.35f;
    const float evo = t * (0.25f + p.noiseEvoSpeed * 12.0f);
    const float dirRate = 0.15f + p.noiseDirChange * 0.02f;
    const float amp = p.displaceStrength * sp * (0.25f + p.displaceBlend * 1.5f);
    const float dispT = t * p.displaceSpeed;

    float er, eg, eb, fr, fg, fb, gr, gg, gb, sr, sg, sb;
    colorToF(p.edgeColor, er, eg, eb);
    colorToF(p.fillColor, fr, fg, fb);
    colorToF(p.glowColor, gr, gg, gb);
    colorToF(p.sparkColor, sr, sg, sb);

    // 闪光色偏：向白光方向偏一点
    sr = mixf(sr, 1.0f, p.sparkHueShift * 0.55f);
    sg = mixf(sg, 0.85f, p.sparkHueShift * 0.55f);
    sb = mixf(sb, 0.55f, p.sparkHueShift * 0.55f);

    const float shinePhase = std::fmod(t * p.shineSpeed * 0.01f, 1.0f);
    const int sparkTick = (int)(t * 6.0f);

    for (int cy = 0; cy < gh; ++cy) {
        for (int cx = 0; cx < gw; ++cx) {
            const size_t i = (size_t)cy * gw + cx;
            const uint8_t st = g.cells[i];
            const float eA = edgeOf[i] ? 1.0f : 0.0f;
            const float gA = glowOf[i];
            if (!st && eA == 0.0f && gA <= 0.0f) { out[i] = 0; continue; }

            // ——±45° 双向置换后的噪声（噪声强度=0 时退化成均匀填充）——
            const float ux = ((float)cx + 0.5f) / (float)gw;
            const float uy = ((float)cy + 0.5f) / (float)gh;
            const float d1 = vnoise(ux * freq + dispT * 0.07f, uy * freq + evo * 0.30f, 7) - 0.5f;
            const float d2 = vnoise(ux * freq + 9.1f, uy * freq - evo * 0.27f, 8) - 0.5f;
            const float nA = fbm(ux + d1 * amp, uy + d1 * amp, 3, 1);
            const float nB = fbm(ux + d2 * amp, uy - d2 * amp, 3, 2);
            float n = (nA + nB) * 0.5f;
            n = mixf(n, 0.5f, smoothMix * 0.5f);
            const float shape = clampf((n - 0.5f) * 2.0f, -1.0f, 1.0f);

            // 逐格闪烁（随时间演化）
            const float flick = vnoise((float)cx * 0.36f + evo * 0.5f,
                                       (float)cy * 0.36f - evo * 0.7f, 3) - 0.5f;
            const float blockFlick = hashf(cx, cy, 900 + (int)(t * dirRate)) - 0.5f;

            float bright = 0.85f + sp * (0.85f * shape + 0.45f * flick)
                         + sp * 0.45f * blockFlick;

            // 斜向扫光
            float dd = ((float)cx / (float)gw + (float)cy / (float)gh) * 0.5f - shinePhase;
            if (dd > 0.5f) dd -= 1.0f;
            if (dd < -0.5f) dd += 1.0f;
            const float shineA = std::exp(-(dd * dd) / 0.010f) * p.shineBrightness;

            float fillA = 0.0f, edgeA = 0.0f, glowA = 0.0f, sparkA = 0.0f;
            if (st) {
                fillA = clampf(p.fillOpacity * 0.46f * bright, 0.0f, 1.0f);
                // 稀疏闪光
                uint32_t hh = ihash((uint32_t)cx * 0x9E3779B1u ^ (uint32_t)cy * 0x85EBCA77u ^
                                    (uint32_t)sparkTick * 0xC2B2AE3Du);
                float sv = (float)(hh & 0xffffu) / 65535.0f;
                if (sv > 0.982f)
                    sparkA = clampf((sv - 0.982f) / 0.018f * p.sparkOpacity * 0.11f, 0.0f, 1.0f);
            }
            if (eA > 0.0f) edgeA = clampf(p.edgeOpacity * 0.85f * (0.42f + 0.52f * bright), 0.0f, 1.0f);
            if (gA > 0.0f) glowA = clampf(gA * 2.10f * p.glowIntensity * bright, 0.0f, 1.0f);

            float A = fillA + edgeA + glowA + sparkA + shineA;
            if (A <= 0.002f) { out[i] = 0; continue; }

            float r = (fr * fillA + er * edgeA + gr * glowA + sr * sparkA + shineA) / A;
            float gg2 = (fg * fillA + eg * edgeA + gg * glowA + sg * sparkA + shineA) / A;
            float b = (fb * fillA + eb * edgeA + gb * glowA + sb * sparkA + shineA) / A;

            // 反相/减色区：官方 disabled 材质偏暗
            if (st && !(st & 1) && (st & 2)) {
                r *= 0.62f; gg2 *= 0.62f; b *= 0.62f;
                A *= 0.85f;
            }

            float a = clampf(A, 0.0f, 1.0f);
            out[i] = ((uint32_t)clampByte(r * a) << 16) |
                     ((uint32_t)clampByte(gg2 * a) << 8) |
                     (uint32_t)clampByte(b * a) |
                     ((uint32_t)clampByte(a) << 24);
        }
    }
}

void expandCells(const Grid& g, const std::vector<uint32_t>& cells,
                 uint32_t* dst, int w, int h, int stridePx) {
    if (!dst || w <= 0 || h <= 0 || g.gw <= 0 || g.gh <= 0) return;
    if (cells.empty()) return;

    static std::vector<int>      colCell;
    static std::vector<uint32_t> row;
    colCell.resize((size_t)w);
    row.resize((size_t)w);
    for (int x = 0; x < w; ++x) {
        int cx = (int)((int64_t)x * g.gw / w);
        colCell[(size_t)x] = cx >= g.gw ? g.gw - 1 : cx;
    }
    int lastCy = -1;
    for (int y = 0; y < h; ++y) {
        int cy = (int)((int64_t)y * g.gh / h);
        if (cy >= g.gh) cy = g.gh - 1;
        if (cy != lastCy) {
            const uint32_t* srcRow = &cells[(size_t)cy * g.gw];
            for (int x = 0; x < w; ++x) row[(size_t)x] = srcRow[colCell[(size_t)x]];
            lastCy = cy;
        }
        std::memcpy(dst + (size_t)y * stridePx, row.data(), (size_t)w * sizeof(uint32_t));
    }
}
