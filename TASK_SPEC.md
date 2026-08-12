# Apex Arc Assist — Implementation Contract

## Goal
Windows x64 native high-arc angle calculator with a readable, semi-transparent
distance ruler overlay. No screen capture, no OCR, no game access.

Formula (must never be replaced by the low branch `degrees(asin(y/60))/2`):

`x = 90 - degrees(asin(y / 60)) / 2`

- `x`: upward elevation in degrees, 45-90.
- `y`: distance in metres, 1-60.
- Anchors: 30m -> 75 degrees; 60m -> 45 degrees.

## Isolation boundary
The application may only:
- register hotkeys (F8 distance input, F9 ruler toggle, Ctrl+Shift+Q exit),
- calculate locally,
- show its own topmost click-through layered overlay,
- read/write one INI next to the exe.

It must never perform screen capture of any kind, use Windows OCR, GPU/video
pipelines (Direct3D/DXGI/WGC/Media Foundation/OBS), process/memory access,
injection, hooks, drivers, network, synthetic input, or game-window messages.

## Interaction
- F9: toggle the ruler panel. It starts hidden and stays hidden until F9.
- F8: enter distance. Digit/numpad/decimal/Backspace/Enter/Esc hotkeys are
  active only while entering. Enter commits 1-60m (decimal allowed); empty
  Enter reuses the last distance; Esc cancels.
- Ctrl+Shift+Q: exit.
- The user reads the panel and aims/throws manually.

## Ruler panel
- Top-right of the work area, always-on-top, `WS_EX_NOACTIVATE` +
  `WS_EX_TRANSPARENT` + `WM_NCHITTEST -> HTTRANSPARENT` (click-through).
- Semi-transparent: layered window alpha so game data behind remains readable.
- Shows 10/20/30/40/50/60m -> 85.2/80.3/75.0/69.1/61.8/45.0 degrees with large
  angle glyphs.
- When a distance was entered, that row is highlighted and the panel header
  shows "current target" distance -> angle.
- No timers, no capture, no polling: the process is idle until a hotkey.

## Structure
- `src/arc_core.h`: portable high-arc formula + tolerance (copy of
  `original/arc_core.h`, extended only with shared constants).
- `src/apex_arc_assist.cpp`: Win32 window, hotkeys, INI persistence, ruler
  painting.
- `resources/app.manifest`, `resources/ApexArcAssist.rc`.
- `build-windows.cmd`: MSVC build into `dist/ApexArcAssist.exe`.
- `.github/workflows/windows-build.yml`: manual `workflow_dispatch` MSVC build,
  core tests, PE import audit, SHA-256, artifact upload.
- `tests/test_arc_core.cpp`: formula anchors/invalid/tolerance/ruler-table.
- `tests/baseline_test.cpp`: compiles `original/arc_core.h` unchanged,
  guarding 30m -> 75.0000.

## Verification
Core tests cover 0/1/30/40/55/60m anchors, invalid inputs, anti-low-branch
guard, monotonicity, derivative tolerance clamps, and the 10-60m ruler table.
Static checks prove no prohibited capture/process/GPU/network API names in
runtime source. Windows build/import audit runs only via manual CI.
