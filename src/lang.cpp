#include "lang.h"

int g_lang = 0;

struct L2 { const wchar_t* zh; const wchar_t* en; };

static const L2 kStr[S_COUNT] = {
    /* S_APP_TITLE */        { L"噪域画笔", L"Noise Painter" },

    /* S_MENU_FILE */        { L"文件", L"File" },
    /* S_MENU_VIEW */        { L"视图", L"View" },
    /* S_MENU_HELP */        { L"帮助", L"Help" },
    /* S_MENU_SAVE */        { L"保存", L"Save" },
    /* S_MENU_LOAD */        { L"读取", L"Reload" },
    /* S_MENU_CLEAR */       { L"清空噪域", L"Clear area" },
    /* S_MENU_RESET */       { L"恢复默认设置", L"Reset to defaults" },
    /* S_MENU_QUIT */        { L"退出", L"Exit" },
    /* S_MENU_DARK */        { L"深色主题", L"Dark theme" },
    /* S_MENU_LIGHT */       { L"浅色主题", L"Light theme" },
    /* S_MENU_LEFT */        { L"仅左屏", L"Left screen only" },
    /* S_MENU_RIGHT */       { L"仅右屏", L"Right screen only" },
    /* S_MENU_SAME */        { L"左右同内容", L"Mirror both screens" },
    /* S_MENU_SPAN */        { L"左右跨区", L"Span across screens" },
    /* S_MENU_WIZARD */      { L"播放向导", L"Play wizard" },
    /* S_MENU_ABOUT */       { L"关于", L"About" },

    /* S_TAB_LOOK */         { L"外观", L"Look" },
    /* S_TAB_INNER */        { L"内部", L"Inside" },
    /* S_TAB_NOISE */        { L"噪域", L"Noise" },
    /* S_TAB_DISPLAY */      { L"显示", L"Display" },

    /* S_TOOL_BRUSH */       { L"画笔", L"Brush" },
    /* S_TOOL_RECT */        { L"矩形", L"Rect" },
    /* S_TOOL_FILL */        { L"填充", L"Fill" },
    /* S_TOOL_ERASER */      { L"橡皮", L"Eraser" },
    /* S_INVERT */           { L"反相(减色)", L"Invert (dim)" },
    /* S_BRUSH */            { L"笔刷", L"Brush" },

    /* S_BTN_CLEAR */        { L"清空", L"Clear" },
    /* S_BTN_SAVE */         { L"保存", L"Save" },
    /* S_BTN_LOAD */         { L"读取", L"Load" },
    /* S_BTN_RESET */        { L"恢复默认设置", L"Reset defaults" },

    /* S_SCREEN_FMT */       { L"显示器：%s", L"Display: %s" },
    /* S_THEME_FMT */        { L"主题：%s", L"Theme: %s" },
    /* S_THEME_DARK */       { L"深色", L"Dark" },
    /* S_THEME_LIGHT */      { L"浅色", L"Light" },
    /* S_MODE_LEFT */        { L"仅左屏", L"Left only" },
    /* S_MODE_RIGHT */       { L"仅右屏", L"Right only" },
    /* S_MODE_SAME */        { L"左右同内容", L"Mirrored" },
    /* S_MODE_SPAN */        { L"左右跨区", L"Span" },

    /* S_CHK_SHOW */         { L"显示噪域", L"Show overlay" },
    /* S_CHK_TOPMOST */      { L"总在最前", L"Always on top" },
    /* S_CHK_PLAY */         { L"循环播放", L"Animate" },

    /* S_HINT_CANVAS */      { L"提示：在画布上左键绘制、右键擦除；覆盖层鼠标穿透，点击会落到下面的窗口。",
                               L"Tip: left-drag to paint, right-click to erase. The overlay is click-through." },
    /* S_HINT_NOISE */       { L"噪声强度是噪域的核心：0 = 均匀填充，越大块噪越强；边缘抖动让遮罩边界随时爬动。",
                               L"Noise strength is the core knob: 0 = flat fill. Edge jitter makes the border crawl." },
    /* S_HINT_DISP1 */       { L"显示器模式决定噪域画在哪块屏：仅左屏 / 仅右屏 / 左右同内容 / 左右跨区",
                               L"Display mode picks where the area lives: left / right / mirrored / span." },
    /* S_HINT_DISP2 */       { L"左右跨区时遮罩覆盖整块虚拟桌面，可以横跨两台显示器画。",
                               L"Span mode makes the mask cover the whole virtual desktop across both screens." },

    /* S_COLOR */            { L"颜色", L"Colors" },
    /* S_COLOR_EDGE */       { L"边缘", L"Edge" },
    /* S_COLOR_FILL */       { L"填充", L"Fill" },
    /* S_COLOR_GLOW */       { L"光晕", L"Glow" },
    /* S_COLOR_SPARK */      { L"闪光", L"Spark" },

    /* S_WIZ_TITLE */        { L"欢迎使用噪域画笔", L"Welcome to Noise Painter" },
    /* S_WIZ_BACK */         { L"返回", L"Back" },
    /* S_WIZ_NEXT */         { L"下一步", L"Next" },
    /* S_WIZ_START */        { L"开始使用", L"Get started" },
    /* S_WIZ_LANG_HINT */    { L"请选择界面语言 / Choose your language", L"请选择界面语言 / Choose your language" },
    /* S_WIZ_TUT_TITLE */    { L"使用向导", L"Quick start" },

    /* S_MSG_RESET_TITLE */  { L"恢复默认设置", L"Reset to defaults" },
    /* S_MSG_RESET_TEXT */   { L"恢复默认设置会清空当前噪域与全部参数，并删除 config.json。\n之后会立刻写入一份默认配置，所以不会重新播放向导。继续？",
                               L"Resetting clears the current area and every parameter, and deletes config.json.\nA fresh default config is written right away, so the wizard will not replay. Continue?" },
    /* S_MSG_ABOUT_TITLE */  { L"关于", L"About" },
    /* S_MSG_ABOUT_TEXT */   { L"噪域画笔（Windows 版）\n\n"
                               L"桌面置顶、鼠标穿透的 block-area 噪域覆盖层，\n"
                               L"纯 Win32 + GDI 实现，无第三方依赖。\n\n"
                               L"C++17 · Windows 10/11 x64",
                               L"Noise Painter (Windows)\n\n"
                               L"A topmost, click-through block-area noise overlay.\n"
                               L"Pure Win32 + GDI, no third-party dependencies.\n\n"
                               L"C++17 · Windows 10/11 x64" },

    /* S_TUT_TEXT */
    { L"一、这是什么\r\n"
      L"一个置顶、鼠标可穿透的透明层，直接在屏幕上盖出块状噪域；覆盖层不挡点击，可以边看网页边折腾。\r\n"
      L"\r\n"
      L"二、画噪域\r\n"
      L"1. 上方画布是整屏（或所选显示器）的缩略图，直接在它上面画。\r\n"
      L"2. 工具：画笔（拖）、矩形（拖框）、填充（连通区）、橡皮（左键拖 / 右键单点）。\r\n"
      L"3. 勾上「反相(减色)」画出来的区块会暗一档。\r\n"
      L"4. 笔刷滑块 = 半径（单位：格）。\r\n"
      L"\r\n"
      L"三、参数\r\n"
      L"· 外观：边缘不透明度 / 光晕亮度 / 边缘强度（环带宽窄）/ 格子像素（最小方块边长）/ 四个颜色。\r\n"
      L"· 内部：填充强度 / 扭曲强度 / 位移混合 / 位移速度 / 闪光色偏 / 闪光明度 / 扫光速度 / 扫光亮度。\r\n"
      L"· 噪域：噪声强度（核心，0 = 均匀填充）/ 噪声半径（团块大小）/ 噪声平滑度 /\r\n"
      L"  噪声演化速度 / 方向变化速度 / 边缘抖动（边界爬动的幅度）。\r\n"
      L"· 显示：显示器模式 / 深浅主题 / 显示噪域 / 总在最前 / 循环播放 / 播放速度。\r\n"
      L"\r\n"
      L"四、显示器模式\r\n"
      L"· 仅左屏 / 仅右屏：只在某一台显示器上显示。\r\n"
      L"· 左右同内容：两台都显示同一份噪域（按各自分辨率缩放）。\r\n"
      L"· 左右跨区：把整个虚拟桌面当成一张画布，可以横跨两台显示器。\r\n"
      L"\r\n"
      L"五、文件\r\n"
      L"· config.json：参数与状态（含语言、firstLaunch）。\r\n"
      L"· mask.dat：画好的噪域遮罩。\r\n"
      L"· 关窗口时自动保存；「恢复默认设置」会删掉 config.json 并立刻补一份默认值。\r\n"
      L"\r\n"
      L"六、菜单\r\n"
      L"· 文件：保存 / 读取 / 清空噪域 / 恢复默认设置 / 退出\r\n"
      L"· 视图：深色 / 浅色、显示器模式\r\n"
      L"· 帮助：播放向导 / 关于\r\n"
      L"\r\n"
      L"提示：控制面板在覆盖层上是挖空的，不会挡住自己的界面；面板最小化后噪域会铺满整屏。\r\n",
      L"1. What it is\r\n"
      L"A topmost, click-through transparent layer that paints a blocky noise area over your desktop.\r\n"
      L"It never steals clicks, so you can keep working underneath.\r\n"
      L"\r\n"
      L"2. Painting\r\n"
      L"1. The canvas above is a thumbnail of the whole screen (or the selected display); paint on it.\r\n"
      L"2. Tools: brush (drag), rect (drag a box), fill (connected region), eraser (drag / right-click).\r\n"
      L"3. With \"Invert (dim)\" the painted blocks render one step darker.\r\n"
      L"4. The brush slider sets the radius in cells.\r\n"
      L"\r\n"
      L"3. Parameters\r\n"
      L"- Look: edge opacity, glow intensity, edge strength (ring width), block size, four colors.\r\n"
      L"- Inside: fill strength, warp strength, warp blend, warp speed, spark hue shift,\r\n"
      L"  spark brightness, shine speed, shine brightness.\r\n"
      L"- Noise: noise strength (the core knob, 0 = flat fill), noise radius (blob size),\r\n"
      L"  smoothness, evolve speed, direction change speed, edge jitter (how far the border crawls).\r\n"
      L"- Display: display mode, theme, show overlay, always on top, animate, playback speed.\r\n"
      L"\r\n"
      L"4. Display modes\r\n"
      L"- Left / Right only: draw on a single monitor.\r\n"
      L"- Mirrored: both monitors show the same area, scaled to their own resolution.\r\n"
      L"- Span: the whole virtual desktop becomes one canvas across both screens.\r\n"
      L"\r\n"
      L"5. Files\r\n"
      L"- config.json: parameters and state (language, firstLaunch).\r\n"
      L"- mask.dat: the painted mask.\r\n"
      L"- Everything is saved when the window closes; \"Reset defaults\" deletes config.json\r\n"
      L"  and immediately writes a default one.\r\n"
      L"\r\n"
      L"6. Menus\r\n"
      L"- File: save / reload / clear / reset defaults / exit\r\n"
      L"- View: dark / light, display mode\r\n"
      L"- Help: play wizard / about\r\n"
      L"\r\n"
      L"Tip: the control panel is punched out of the overlay, so it never hides itself;\r\n"
      L"minimize the panel and the area fills the whole screen.\r\n" },

    /* slider labels */
    { L"边缘不透明度", L"Edge opacity" },
    { L"光晕亮度", L"Glow intensity" },
    { L"边缘强度", L"Edge strength" },
    { L"格子像素", L"Block size" },
    { L"填充强度", L"Fill strength" },
    { L"扭曲强度", L"Warp strength" },
    { L"位移混合", L"Warp blend" },
    { L"位移速度", L"Warp speed" },
    { L"闪光色偏", L"Spark hue" },
    { L"闪光明度", L"Spark level" },
    { L"扫光速度", L"Shine speed" },
    { L"扫光亮度", L"Shine level" },
    { L"噪声强度", L"Noise strength" },
    { L"噪声半径", L"Noise radius" },
    { L"噪声平滑度", L"Noise smooth" },
    { L"噪声演化速度", L"Noise evolve" },
    { L"方向变化速度", L"Direction speed" },
    { L"边缘抖动", L"Edge jitter" },
    { L"播放速度", L"Playback speed" },
};

const wchar_t* T(int id) {
    if (id < 0 || id >= S_COUNT) return L"";
    return g_lang ? kStr[id].en : kStr[id].zh;
}
