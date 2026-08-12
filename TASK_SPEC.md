# Apex Arc Assist — Implementation Contract

## Goal
Build a portable-source Windows x64 helper for the user's explicit high-arc formula:

`x = 90 - degrees(asin(y / 60)) / 2`

- `x`: upward elevation in degrees, high branch, 45–90 degrees.
- `y`: ping distance in metres, valid range 1–60 for the UI.
- Anchor: 30m -> 75 degrees; 60m -> 45 degrees.

Do not replace this with the low branch `degrees(asin(y/60))/2`.

## Isolation boundary
The application may only:
- on explicit F5/F8 request only, scan each display's bounded upper-left region with Win32 GDI (`GetDC`, `BitBlt`) to locate the `ANG:` label; then capture only the resulting tiny pitch ROI,
- classify the fixed `-+.0123456789` glyph set locally with embedded screenshot-derived Apex HUD templates, generated fallback templates, and optional user-trained binary templates,
- register user hotkeys, calculate locally, show its own topmost overlay, and play a notification.

Runtime code must not use Windows.Media.Ocr, Direct3D, DXGI, Windows Graphics Capture, Media Foundation, OBS integration, or any other GPU/video pipeline. OBS is deliberately not an MVP dependency because its capture/composition process is heavier than one tiny GDI ROI.

It must never use process access, memory access, DLL injection, hooks, drivers, network traffic, or synthetic mouse/keyboard output. In particular, source/imports must not contain: `OpenProcess`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAllocEx`, `CreateRemoteThread`, `SetWindowsHookEx`, `SendInput`, `keybd_event`, `mouse_event`, `SetCursorPos`, or messages sent to the game.

## MVP interaction
- F8: first make one bounded automatic scan for `ANG:` and its first pitch token, then enter distance input. While active, dynamically register 0–9/numpad, decimal point, Backspace, Enter, Esc as hotkeys. Enter commits 1–60m; empty Enter reuses the last distance. If auto-location fails, retain the prior ROI and show an honest fallback message.
- F9: ROI calibration mode. Show a visible rectangle around only the first pitch number in the `ang:` line. Arrow keys move 1px, Shift+arrow 10px; minus/numpad-minus shrink the rectangle around its centre, equals/plus/numpad-plus grow it, Home restores the recommended compact 72x18 size, and Ctrl+arrow performs 1px width/height adjustment. Resize/move keys repeat while held, the ROI is clamped to the virtual desktop after every adjustment/display change, and registration failures are shown in the overlay. Enter saves; Esc restores.
- F5: one-shot automatic `ANG:` locator. Scan no more than 720x280 pixels at the upper-left of each display, match the screenshot-derived `ANG` glyphs, lock the first complete pitch token, release the larger DIB, and persist the resulting small ROI. Shift+F5 is the optional former template-learning fallback: capture the current ROI once, then let the user type the exact visible pitch text and press Enter.
- F6: pause/resume monitoring immediately. Paused means zero screen capture.
- F7: toggle cl_showpos convention between `up is negative` (default, Source convention) and `up is positive`.
- F10: toggle sound.
- Ctrl+Shift+Q: exit.
- User always aims and throws manually.

## Ultra-light capture and glyph recognition
- IDLE, distance input, paused, and untrained states perform zero periodic `BitBlt` calls. F5/F8 auto-location is an explicit one-shot exception; after it completes, the process waits for messages/hotkeys until ARMED.
- Native GDI normally captures only the automatically located/calibrated pitch token ROI: default minimum 72x18 physical pixels; recommended final ROI 48–96px wide and 12–24px tall. The explicit auto-location scan is capped at 720x280 per display and releases its larger DIB before normal monitoring. Existing INI values remain valid.
- Do not upscale at runtime. For one F5/F8 GDI frame, derive bright/near-neutral masks at 150, 160 and 170, plus the current F9 threshold when different; successful masks must corroborate one `ANG:` token/ROI before selecting the highest-confidence candidate. Default chroma delta is 64; saturated fallback requires high>=248 and low>=190. Crop connected glyph components, normalize each glyph to a small binary grid, and match screenshot-derived Apex HUD templates. An auto-located ROI must fail closed rather than fall back to generic templates; only F9/Shift+F5 manual paths may use startup-generated common-font/optional exact templates. Grammar accepts only a complete `[+-]?dd.dd`-style numeric token and rejects values outside a physical pitch range.
- Preserve pitch sign safety: every nonzero automatic token requires a visible, unmerged `+/-` and an uncropped token boundary. An unsigned opposite-sign reading, missing/merged sign, internal suffix, ROI-edge crop, or unsigned nonzero value after a zero crossing must be rejected as OCR loss rather than reverse the user-facing guidance.
- Hash the binary ROI; if unchanged, reuse the previous parse without template matching.
- Adaptive schedule: FAR 5Hz, within 8 degrees 12Hz, READY 3Hz, OCR-lost exponential backoff 4Hz -> 2Hz -> 1Hz. No timer-resolution changes and no queued capture jobs.
- Normalize 0–360 readings to signed degrees, then map to upward elevation according to the selected sign convention.
- Stabilize valid samples, reject implausible jumps/stale data, and use a distance-sensitive READY tolerance with a 0.35 degree floor. Show OCR confidence and runtime capture/match microseconds.

## Performance budget (P0)
- IDLE/paused: no capture and no polling loop; Task Manager should round to 0.0% after settling.
- ARMED: median capture+classify time <= 1.5ms/sample and p95 <= 3ms/sample; default duty cycle <= 12 samples/s only near target.
- READY: <= 3 samples/s, edge beep only.
- GPU APIs/imports: zero; GPU engine utilization attributable to this process: zero.
- Memory working set target: < 10MB; no third-party runtime or background service.
- Frame-time acceptance on the user's PC: median FPS delta <= 1% and p95 frame-time delta <= 0.5ms in three 60-second A/B runs.

## Overlay
Topmost, no-activate, click-through own window. Show:
- distance,
- target upward elevation,
- target cl_showpos pitch,
- current OCR pitch and mapped upward elevation,
- signed error/direction,
- OCR state and READY state.

READY enters after two in-window samples spanning at least 120ms and at most 500ms; the first hit schedules the next normal near-rate sample. It exits after two samples beyond `2 * tolerance`. Tolerance is `clamp(0.5 * |dx/dy|, 0.35, 2.0)` degrees so integer ping rounding remains usable near 60m. Beep only on the entry edge.

## Structure
- Preserve `original/arc_core.h` unchanged.
- Copy it to `src/arc_core.h` and extend only the copy with portable formula, pitch normalization/mapping, OCR text number extraction, and READY gate.
- `src/apex_arc_assist.cpp`: Windows app, one-shot bounded ANG auto-location, tiny GDI capture, embedded/generated/user-trained template OCR, adaptive scheduler, overlay, and hotkeys.
- `src/apex_hud_ocr.h`: portable ANG label location, sign-shape validation, and screenshot-derived Apex HUD glyph helpers; runtime never opens image files.
- `src/roi_geometry.h`: portable centred ROI resize, compact reset, dimension clamp, and virtual-desktop bounds clamp helpers.
- `resources/app.manifest`, `resources/ApexArcAssist.rc`.
- `build-windows.cmd`: direct MSVC/Windows SDK build into `dist/ApexArcAssist.exe`.
- `.github/workflows/windows-build.yml`: Windows `windows-latest` MSVC build, core test, PE import audit, SHA-256, and artifact upload.
- `tests/test_arc_core.cpp` and `tests/fixtures/apex_hud_pic_2560x1440_mask.h`: macOS/Linux/Windows pure-core tests including a privacy-minimized binary `ANG: -52.25` screenshot-derived regression fixture.
- `README.md`: Chinese setup/use/build explanation, including `+cl_showpos 1`, F8/F5 automatic ANG location, F9 fallback calibration, sign toggle, full-screen GDI limitation, and explicit isolation boundary.
- `SECURITY_BOUNDARY.md`: auditable API boundary.

## Verification
Core tests must cover 0, 30, 40, 55, 60m anchors, invalid values, derivative-based tolerance, 0/360 normalization, both sign modes, pitch-sign continuity/fail-closed behavior, numeric grammar, screenshot-derived numeric glyph matching, the automatic `ANG: -52.25` locate/read path at original, 75%, and 2x HUD scales, non-zero raster phases, blank-scan rejection, missing/merged-minus rejection, right-shifted ROI suffix rejection, same-frame multi-threshold corroboration/conflict rejection, binary hash reuse, centred ROI resize/compact/clamp behavior, READY enter/exit hysteresis, and stale/invalid reset. Build the core test plus `baseline_test.cpp` with clang++ in the current macOS environment. Static checks must prove that no prohibited process/input/GPU/OCR APIs appear in runtime source or the PE import table.
