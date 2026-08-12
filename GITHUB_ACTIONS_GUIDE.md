# GitHub Actions 手动编译指南（Windows x64 / MSVC）

本项目的工作流是**仅手动触发**：上传、push、提交 PR 都不会自动编译。只有你在 GitHub Actions 页面点击 **Run workflow** 后，才会启动 `windows-latest` Runner。

工作流文件：`.github/workflows/windows-build.yml`

它会依次完成：

1. 扫描运行时源码，禁止进程/内存访问、Hook、输入合成、GPU/视频管线、系统 OCR、网络 API；
2. 用 MSVC x64 编译并执行纯核心测试（测试数量会随功能增加；以日志 `arc_core: N checks, 0 failures` 为准）；
3. 调用 `build-windows.cmd` 编译 `dist/ApexArcAssist.exe`；
4. 用 `dumpbin` 审计 PE 导入表；
5. 生成 `ApexArcAssist.exe.sha256`；
6. 上传名为 `ApexArcAssist-windows-x64` 的 artifact（保留 30 天）。

---

## 路线 A：使用 Git 命令上传（推荐）

### 1. 在 GitHub 创建空仓库

1. 打开 <https://github.com/new>。
2. Repository name 可填写 `ApexArcAssist`。
3. Public/Private 均可。
4. **不要勾选** README、`.gitignore` 或 License（保持空仓库）。
5. 点击 **Create repository**。

### 2. 解压源码包并检查隐藏工作流

源码包解压后，目录根部必须能看到：

```text
.github/workflows/windows-build.yml
src/apex_arc_assist.cpp
src/apex_hud_ocr.h
src/arc_core.h
resources/ApexArcAssist.rc
resources/app.manifest
build-windows.cmd
README.md
```

macOS Finder 默认不突出显示 `.github`，终端中用以下命令确认：

```bash
cd /解压后的/ApexArcAssist
ls -la .github/workflows
```

预期包含 `windows-build.yml`。

### 3. 手动提交并推送

把下面 URL 换成你刚创建的仓库地址：

```bash
cd /解压后的/ApexArcAssist
git init -b main
git add .
git commit -m "Add Apex Arc Assist"
git remote add origin https://github.com/你的用户名/ApexArcAssist.git
git push -u origin main
```

如果 Git 尚未设置身份，只需在这个仓库内设置：

```bash
git config user.name "你的 GitHub 用户名"
git config user.email "你的 GitHub 邮箱"
```

然后重新执行 `git commit` 与 `git push`。

---

## 路线 B：只使用 GitHub 网页

GitHub 网页上传时容易漏掉隐藏目录 `.github`，建议按此顺序：

1. 创建仓库后，点击 **Add file → Upload files**，上传源码包内除 `.github` 外的目录和文件并 Commit。
2. 点击 **Add file → Create new file**。
3. 文件名完整填写：

   ```text
   .github/workflows/windows-build.yml
   ```

4. 打开本地同名文件，完整复制内容到网页编辑器。
5. 点击 **Commit changes**。
6. 进入仓库 **Actions** 页面，确认左侧出现 **Build Windows x64**。

如果 Actions 页面提示需要启用工作流，点击 **I understand my workflows, go ahead and enable them**。

---

## 手动运行 Windows 编译

1. 打开你的仓库。
2. 点击顶部 **Actions**。
3. 左侧选择 **Build Windows x64**。
4. 右侧点击 **Run workflow**。
5. Branch 选择包含源码的分支（通常是 `main`）。
6. 再点击绿色的 **Run workflow**。
7. 等待新的 run 出现，进入该 run，确认 `windows-x64` 全部步骤变绿。

重点日志应包含：

```text
FORBIDDEN_API_SCAN_OK runtime_source=src,resources
arc_core: 750 checks, 0 failures
BASELINE_OK distance=30.0000 high_arc=75.0000
DOC_VERIFY_OK documents=14 core_checks=750 workflow=manual_only
BUILT: ...\dist\ApexArcAssist.exe
PE_IMPORT_SCAN_OK required=USER32.dll,GDI32.dll forbidden_gpu_network_video=none
SHA256=...
```

---

## 下载编译产物

1. 打开刚完成的 workflow run。
2. 滚动到页面底部 **Artifacts**。
3. 点击 `ApexArcAssist-windows-x64` 下载 ZIP。
4. 解压后应包含：

```text
ApexArcAssist.exe
ApexArcAssist.exe.sha256
imports.txt
dependents.txt
README.md
SECURITY_BOUNDARY.md
PERFORMANCE_TEST.md
```

Artifact 默认保留 30 天；需要长期保存时请下载到本地或创建 GitHub Release。

---

## 在 Windows 上核验 SHA-256

在解压目录打开 PowerShell：

```powershell
$actual = (Get-FileHash .\ApexArcAssist.exe -Algorithm SHA256).Hash.ToLowerInvariant()
$expected = ((Get-Content .\ApexArcAssist.exe.sha256 -Raw) -split '\s+')[0].ToLowerInvariant()
"actual  = $actual"
"expected= $expected"
if ($actual -ne $expected) { throw "SHA-256 不一致" }
"SHA256_OK"
```

预期最后输出：

```text
SHA256_OK
```

查看导入审计：

```powershell
Get-Content .\imports.txt
Get-Content .\dependents.txt
```

工作流已经自动要求 `USER32.dll`、`GDI32.dll` 存在，并拒绝 D3D/DXGI/媒体/网络/OBS 相关 DLL。

---

## 首次运行顺序

1. Apex 启动项加入 `+cl_showpos 1`，确认左上角出现 `NAME / POS / ANG / VEL`。
2. 运行 `ApexArcAssist.exe`。可以先在你实际使用的全屏模式测试；前提是现有 GDI 路径确实能读到游戏 HUD。
3. `F8`：程序会一次性自动扫描每块显示器左上角、找到 `ANG:` 后第一个 pitch，并在悬浮窗显示找到的 token。
4. 输入 1–60m 距离并 Enter；**无需手输任何 pitch 数字**。
5. `F7`：若抬头时读数方向与默认不一致，切换符号约定。
6. 自己移动准星；进入 READY 后自己投掷。
7. `F5`：仅在自动定位失败、HUD 移动或显示模式变化后，用于强制重新自动定位。
8. 仍失败时才用 `F9` 手调 ROI；`Home` 恢复 `72×18`，`-` 缩小，`=`/`+` 放大。`Shift+F5` 是可选的人工模板补强。
9. `F6`：暂停/恢复；暂停时为零周期屏幕采集。

完整操作见 `README.md`，实机性能 A/B 验收见 `PERFORMANCE_TEST.md`。

---

## 常见构建问题

### Actions 左侧没有 Build Windows x64

检查默认分支上是否真实存在：

```text
.github/workflows/windows-build.yml
```

注意目录名和扩展名必须完全一致。

### workflow 没有自动运行

这是预期行为。当前 YAML 只有 `workflow_dispatch`，必须手动点击 **Run workflow**。

### 日志显示 `The system cannot find the path specified`，且尚未出现 `cl.exe`

旧版工作流曾把 Visual Studio 写死为 `2022\\Enterprise`。当前版本已改用 Runner 自带的 `vswhere.exe` 自动定位任何包含 `VC.Tools.x86.x64` 的 Visual Studio，并会在日志打印 `VSINSTALL=...`、`where cl.exe`、`where rc.exe`、`where dumpbin.exe`。请确认仓库中的 workflow 已更新后重新手动运行。

### Node.js 20 deprecated 注解

当前工作流使用 `actions/checkout@v7` 与 `actions/upload-artifact@v7`（Node.js 24），不再使用产生该注解的 v4。

### `rc.exe` 找不到 manifest

确认以下两个文件都已上传且大小写一致：

```text
resources/ApexArcAssist.rc
resources/app.manifest
```

### MSVC 核心测试失败

先看日志中的具体 `FAIL 文件:行号`；公式关键锚点必须保持：

```text
30m -> 75.0000°
60m -> 45.0000°
```

### 下载后 Windows 显示来源提示

这是未代码签名的自编译 EXE 常见提示。你可以先核对 workflow、SHA-256 和 `imports.txt`；若文件属性中存在“解除锁定”，可在 PowerShell 执行：

```powershell
Unblock-File .\ApexArcAssist.exe
```
