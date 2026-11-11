#pragma once
#include "noise.h"
#include "overlay.h"
#include <string>
#include <vector>

enum class ScreenMode { Left = 0, Right = 1, Same = 2, Span = 3 };
enum class ThemeKind { Dark = 0, Light = 1 };

struct AppState {
    Params p;
    Grid   grid;                                   // 网格坐标 = 遮罩空间
    ScreenMode screenMode = ScreenMode::Span;      // 左右跨区
    ThemeKind  theme = ThemeKind::Dark;            // 深色
    int        language = 0;                       // 0 中文, 1 English
    double timeSec = 6.0;                          // 与网页初始 S.time = 6 对齐
    bool   dirty = true;
    bool   firstLaunch = false;
    std::vector<uint32_t> cellColors;
};

extern AppState g_app;

std::wstring App_ExeDir();
std::wstring App_ConfigPath();
std::wstring App_MaskPath();

void App_LoadConfig();        // 读 config.json；不存在或 firstLaunch=true → g_app.firstLaunch = true
void App_SaveConfig();
void App_SaveMask();
void App_LoadMask();

// 恢复默认：删掉 config.json，随即补一份默认（firstLaunch=false，所以不会再弹教程）
void App_ResetDefaults();

std::vector<RECT> App_GetMonitors();                       // 按横坐标排序
void App_UpdateMaskSpace();                                // 依屏幕模式确定遮罩空间与 grid 尺寸
std::vector<OverlayTarget> App_OverlayTargets();           // 需要渲染的屏幕矩形
const wchar_t* ScreenModeName(ScreenMode m);
