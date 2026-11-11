# 噪域画笔 · Windows 置顶噪域（NoiseAreaForWindows）

把 [noisepainter.sling.cc.cd](https://noisepainter.sling.cc.cd/) 那套 Phigros / Phira「噪域（block-area）」效果搬到 Windows 桌面：
一个**置顶、鼠标可穿透**的透明覆盖层直接盖在屏幕上；控制面板里改参数、在预览画布上刷区域，支持双屏。

- 语言：C++17，纯 Win32 + GDI，无第三方依赖
- 产物：`build\NoiseArea.exe`（单文件，双击即用）
- 只在 Windows 上运行；多显示器可分别投放

![预览](docs/preview.png)

浅色主题：

![浅色主题](docs/preview-light.png)

## 算法来源

参考站点（Phira / prpr 官方 block-area 实现）的核心管线，参数与语义对齐：

1. **遮罩**：按 `格子像素`（默认 8px，对应官方「屏幕 / 8」）切格，每格 1 字节：`bit0` = 普通噪域区，`bit1` = 反相 / 减色区（官方 disabled 材质）。
2. **±45° 双向噪声置换**：格子中心做两次方向相反的置换后采样 fbm，再叠加逐格闪烁与逐格随机差异，得到噪域的「块噪」。
3. **环带提取**：官方九宫格膨胀 + 环带（bit row dilation）。`GLOW_W[p] = (6-p)^2.65 / Σ k^2.65`（k=1..6）与站点完全一致；`边缘强度` 反向映射成环带占几格（1..6，越大边缘越细）。第一段环带画 `Edge`，全部环带按权重累加 `Glow`。
4. **着色**：`Edge / Fill / Glow / Spark / Shine` 五路按网页的系数合成，预乘 alpha 后上屏。
5. **上屏**：`WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST` + `UpdateLayeredWindow` 逐像素 alpha；`WM_NCHITTEST → HTTRANSPARENT`，点击直接落到下面的窗口。

## 参数

| 分组 | 参数 | 范围 | 默认 | 网页 uniform |
|---|---|---|---|---|
| 外观 | 边缘不透明度 | 0–2 | 0.80 | `_EdgeOpacity` |
| 外观 | 光晕亮度 | 0–3 | 0.80 | `_GlowIntensity` |
| 外观 | 边缘强度 | 1–6 | 6 | （环带宽度） |
| 外观 | 格子像素 | 2–32 | 8 | （1/8 分辨率） |
| 内部 | 填充强度 | 0–1.5 | 0.667 | `_FillOpacity` |
| 内部 | 扭曲强度 | 0–0.5 | 0.15 | `_DisplaceStrength` |
| 内部 | 位移混合 | 0–1 | 0.411 | `_DisplaceBlendIntensity` |
| 内部 | 位移速度 | 0–5 | 1.5 | `_DisplaceSpeed` |
| 内部 | 闪光色偏 | 0–1 | 0.20 | `_SparkHueShiftAmount` |
| 内部 | 闪光明度 | 0–12 | 5.69 | `_SparkMapOpacity` |
| 内部 | 扫光速度 | 0–60 | 37.9 | `_ShineSpeed` |
| 内部 | 扫光亮度 | 0–0.5 | 0.12 | `_ShineBrightness` |
| **噪域** | **噪声强度** | 0–2 | 1.0 | `_NoiseDisplaceStrength` |
| 噪域 | 噪声半径 | 0.1–1.5 | 0.48 | `_NoiseRadius` |
| 噪域 | 噪声平滑度 | 0–2 | 1.0 | `_NoiseSmoothness` |
| 噪域 | 噪声演化速度 | 0–0.2 | 0.03 | `_NoiseEvoSpeed` |
| 噪域 | 方向变化速度 | 0–120 | 60 | `_NoiseDirChangeSpeed` |
| 显示 | 播放速度 | 0–3 | 1.0 | （动画倍速） |

颜色：边缘 `#FF5454` / 填充 `#B63C3C` / 光晕 `#FF2E2E` / 闪光 `#FF4949`，对应 `_EdgeColor` 等。

**噪声强度是噪域的核心**：0 = 均匀填充，越大块噪越强；配合噪声半径（团块大小）与平滑度调出来的手感最接近网页。

## 用法

- **画**：上方画布是整屏（或所选显示器）的缩略图，直接在它上面画，覆盖层实时同步。
  - 画笔（拖）、矩形（拖框）、填充（连通区）、橡皮（左键拖 / 右键单点擦）
  - `反相(减色)` 勾上后画的是暗一档的减色区（网页的 bit1 通道）
  - `笔刷` 滑块 = 半径（单位：格）
- **标签页**：外观 / 内部 / 噪域 / 显示。
- **显示器模式**（显示页或「视图」菜单）：
  - 仅左屏 / 仅右屏：只在某一台显示器上显示
  - 左右同内容：两台都显示同一份噪域（各按自身分辨率缩放）
  - 左右跨区：遮罩覆盖整块虚拟桌面，可以横跨两台显示器
- **主题**：深色 / 浅色（显示页按钮或「视图」菜单）。
- **文件**：`保存 / 读取 / 清空噪域 / 恢复默认设置`；关窗口自动保存，启动自动读取。
- **教程**：首次启动自动弹出；之后从「帮助 → 教程」随时打开。

## 配置文件

放在 exe 同目录：

```jsonc
{
  "version": 2,
  "firstLaunch": false,     // true = 下次启动再弹一次教程
  "theme": "dark",          // dark | light
  "screenMode": "span",     // left | right | same | span
  "enabled": true,
  "topmost": true,
  "playing": true,
  "blockPx": 8,
  "noiseStrength": 1.0,
  "noiseRadius": 0.48,
  // …其余参数同名
  "edge": "#FF5454", "fill": "#B63C3C", "glow": "#FF2E2E", "spark": "#FF4949"
}
```

- `config.json` 不存在时启动 → 视为首次启动，弹教程并写入默认配置。
- 「恢复默认设置」= 运行时删掉 `config.json`，随即补一份默认（`firstLaunch=false`），所以不会重复弹教程。
- 噪域遮罩单独存在 `mask.dat`（换显示器模式/分辨率会按位置/比例重采样保留）。

## 编译

需要 VS2022（MSVC x64），直接跑：

```bat
build.bat
```

脚本会调用 `vcvars64.bat`，用 `cl /std:c++17 /O2 /utf-8 /DNOMINMAX /DUNICODE` 编译 `src\*.cpp` 并链接成 `build\NoiseArea.exe`。

> 源码是 UTF-8（含中文），必须保留 `/utf-8`，否则中文注释在 GBK 代码页下会吞行。

## 文件结构

```
src\main.cpp      入口：DPI 感知、配置/遮罩加载、教程、消息循环
src\control.cpp   控制面板（自绘滑块 / 复选框 / 标签页）、预览编辑画布、教程窗口
src\overlay.cpp   置顶穿透分层窗口（可多块，对应多显示器）
src\noise.cpp     噪域合成：置换噪声 / 环带 / 着色 / 最近邻展开
src\app.cpp       config.json、mask.dat、显示器枚举与遮罩空间
build.bat         一键编译
```

## 与网页版已知差异

- 网页跑官方 GLSL 片元着色器（spark 贴图、辅助遮罩、悬浮 SDF 等分支），这里是 CPU 合成；参数、环带、置换与配色管线一致，spark / 扫光按观感近似。
- 网页画在导入的图片上；这里画在屏幕空间（一「格」= 屏幕像素），所以 `格子像素` 即最小方块边长。

## License

MIT
