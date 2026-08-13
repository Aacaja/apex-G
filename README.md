# Apex Angle Reader — Rust 版高抛距离实时显示

Windows 原生 Rust 工具：实时读取 Apex `cl_showpos` 左上角 HUD 的 `ang:` 行
（纯像素模板匹配，无 OCR 库、无 OpenCV），反解高抛公式后，在半透明悬浮窗中
**直接显示当前角度对应的可抛出距离**。你只需要在游戏里核对距离，决定是否投掷。

## 显示含义

- 大字数字 = 当前 pitch 对应的**高抛距离（米）**
- 小字 = 当前识别的 pitch（抬头为负）
- `-- m` = 未检测到有效 pitch（看地面/菜单/识别失败），保持上一帧数值不误导

## 用法

- 启动 `apex-angle-reader.exe`（需要游戏开启 `+cl_showpos 1`）
- `F9`：显示/隐藏悬浮窗
- `Ctrl+Shift+Q`：退出
- 命令行参数（默认值已按你的 2560×1440 显示器 + 拉伸 2560×1920 配置）：
  - `--monitor N`：捕获哪块显示器（默认 0）
  - `--roi x,y,w,h`：捕获区域（默认 `0,0,320,120`）
  - `--hz 10..240`：捕获频率（默认 60）
  - `--top px`：悬浮窗顶部偏移（默认 96，即 HUD 数字下方）

## 核心逻辑

- 公式（固定契约）：`x = 90 - degrees(asin(y/60)) / 2`；30m→75°、60m→45°
- 反解：`y = 60 × sin(radians((90 + pitch) × 2))`（pitch 抬头为负）
- 校验：−75°→30.0m、−45°→60.0m、−85°→≈10.4m

## 安全边界

- 仅读取屏幕像素（Windows Graphics Capture，由 xcap 提供）并做本地计算
- 无网络、无进程/内存访问、无注入、无 Hook、无合成输入、不写游戏
- 运行时不读取任何图片文件（模板为离线嵌入）

## 构建

- 本仓库在 macOS/Linux 上只运行跨平台测试（`cargo test --lib` 与
  `cargo test --test ocr_test`）；Windows 编译统一由 GitHub Actions 完成
- CI（`.github/workflows/build.yml`）：push 到 main 自动构建
  `x86_64-pc-windows-msvc` release，产物 `apex-angle-reader.exe` 上传为
  `apex-angle-reader` artifact

## 测试

```bash
cargo test --lib          # 公式锚点/反解/无效值
cargo test --test ocr_test # 用离线 pic.png 二值 fixture 验证 ANG 行识别与负号门
```

## 已知限制

- 依赖 Windows Graphics Capture：**独占全屏**下某些驱动/游戏组合可能抓不到帧，
  若出现持续 `--`，请改用无边框窗口化（Apex 支持）
- 模板来自你的 2560×1920 拉伸画面；若缩放模式改变导致识别失败，可从新截图
  重新提取模板（见 `tools/`）
- 距离为 1 位小数显示，供人眼核对，不保证游戏内 ping 取整语义
