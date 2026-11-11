// 置顶、鼠标穿透的分层覆盖层；支持多块屏幕区域（每块一张分层窗口）
#pragma once
#include "noise.h"

struct OverlayTarget { int x, y, w, h; };

bool Overlay_Init();
void Overlay_Shutdown();

// 设定要渲染的屏幕矩形集合（变化时会重建窗口）
void Overlay_SetTargets(const OverlayTarget* t, int n);

// 每块目标都渲染整张遮罩（缩放填满），hole = 屏幕坐标里要留空的区域（控制面板）
void Overlay_Render(const Grid& g, const std::vector<uint32_t>& cells,
                    const RECT& hole, bool holeValid);

void Overlay_Show(bool show);
void Overlay_SetTopmost(bool on);
void Overlay_KeepTopmost();
