# 开发历史

## 早期提交（已在私有仓库）

| Commit | 内容 | 已知验证 |
|---|---|---|
| `9392ea2a6e8081c61247b3bc56184bac92cfd971` | 初始像素级高抛辅助 | 后续被 Windows 构建流程替代 |
| `e5f8eed83468091242e5c3b13fcb8bf5201281c1` | `vswhere` 发现 MSVC、Actions v7 | 手动 run `31500665808` 成功；仅针对该提交 |
| `b2abeb3de86c0c2991416a801388936a65c55459` | F9 紧凑、可缩放 ROI | 当前发布树基线 |

历史失败 run `31499225735` 在硬编码 Visual Studio 路径阶段报
`The system cannot find the path specified.`；修复证据见
`artifacts/workflow-fix/VERIFICATION.txt`。

## 自动 OCR 工作区事务

离线参考是用户本地 `pic.png`（2560×1440；SHA-256
`eb0056aea5990a5695a8c2078a4cbddf60f44d8fa9087d9edf460459fdb13de0`）。
它不进入 Git；运行时从不读取它。

演进要点：

1. 从参考图生成只有 HUD 前景的二值 fixture 和精确字形模板。
2. 首版缩放定位暴露了相邻 POS/VEL 行组合成假 `ANG` 的问题；修复为左列、同行、
   A→N→G、冒号和完整 token 校验。
3. 75% 缩放可将数字笔画分裂；加入仅在确认基线内的片段合并和缩放/相位回归。
4. 发现负号在低分辨率消失会把 `-52.25` 变为无符号角度；自动定位与严格 ARMED
   路径改为非零必须有可见符号。
5. 发现右偏 padded ROI 可从 `-52.25` 取出 `2.25`；加入 token 起止边界、ROI 留边
   和非零无符号拒绝。
6. 本次增加单帧多阈值路径：相同 DIB 上的候选必须同意 token 与 ROI，否则拒绝。

详见 [`CURRENT_STATUS.md`](CURRENT_STATUS.md)、[`PROGRESS.md`](PROGRESS.md) 和
根目录 [`CHANGELOG.md`](../CHANGELOG.md)。
