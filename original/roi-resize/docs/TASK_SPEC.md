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
- capture a user-calibrated, tiny desktop pixel ROI with Win32 GDI (`GetDC`, `BitBlt`),
- classify the fixed `-+.0123456789` glyph set locally with generated and user-trained binary templates,
- register user hotkeys, calculate locally, show its own topmost overlay, and play a notification.

Runtime code must not use Windows.Media.Ocr, Direct3D, DXGI, Windows Graphics Capture, Media Foundation, OBS integration, or any other GPU/video pipeline. OBS is deliberately not an MVP dependency because its capture/composition process is heavier than one tiny GDI ROI.

It must never use process access, memory access, DLL injection, hooks, drivers, network traffic, or synthetic mouse/keyboard output. In particular, source/imports must not contain: `OpenProcess`, `ReadProcessMemory`, `WriteProcessMemory`, `VirtualAllocEx`, `CreateRemoteThread`, `SetWindowsHookEx`, `SendInput`, `keybd_event`, `mouse_event`, `SetCursorPos`, or messages sent to the game.

## MVP interaction
- F8: enter distance input. While active, dynamically register 0–9/numpad, decimal point, Backspace, Enter, Esc as hotkeys. Enter commits 1–60m; empty Enter reuses the last distance.
- F9: ROI calibration mode. Show a visible rectangle around only the first pitch number in the `ang:` line. Arrow keys move 1px, Shift+arrow 10px, Ctrl+arrow resize. Enter saves; Esc restores.
- F5: one-shot template learning. Capture the current ROI once, then let the user type the exact visible pitch text and press Enter. Store the segmented exact glyphs; generated common-font templates remain the zero-setup fallback.
- F6: pause/resume monitoring immediately. Paused means zero screen capture.
- F7: toggle cl_showpos convention between `up is negative` (default, Source convention) and `up is positive`.
- F10: toggle sound.
- Ctrl+Shift+Q: exit.
- User always aims and throws manually.

## Ultra-light capture and glyph recognition
- IDLE, distance input, paused, and untrained states perform zero `BitBlt` calls. The process only waits for messages/hotkeys.
- Native GDI captures only the calibrated pitch token ROI: default 96x24 physical pixels; recommended final ROI 48–96px wide and 12–24px tall. Reuse one screen DC, one memory DC, and one top-down 32-bit DIB for the entire run.
- Do not upscale. Threshold bright HUD text, crop connected glyph components, normalize each glyph to a small binary grid, and match against startup-generated common Windows-font templates plus optional F5 exact templates. Grammar accepts only `[+-]?dd.dd`-style numeric tokens and rejects values outside a physical pitch range.
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
- `src/apex_arc_assist.cpp`: Windows app, tiny GDI capture, generated/user-trained template OCR, adaptive scheduler, overlay, and hotkeys.
- `resources/app.manifest`, `resources/ApexArcAssist.rc`.
- `build-windows.cmd`: direct MSVC/Windows SDK build into `dist/ApexArcAssist.exe`.
- `.github/workflows/windows-build.yml`: Windows `windows-latest` MSVC build, core test, PE import audit, SHA-256, and artifact upload.
- `tests/test_arc_core.cpp`: macOS/Linux/Windows pure-core tests.
- `README.md`: Chinese setup/use/build explanation, including `+cl_showpos 1`, borderless recommendation, F9 calibration, sign toggle, and explicit isolation boundary.
- `SECURITY_BOUNDARY.md`: auditable API boundary.

## Verification
Core tests must cover 0, 30, 40, 55, 60m anchors, invalid values, derivative-based tolerance, 0/360 normalization, both sign modes, numeric grammar, glyph normalization/matching helpers, binary hash reuse, READY enter/exit hysteresis, and stale/invalid reset. Build the core test with clang++ in the current macOS environment. Static checks must prove that no prohibited process/input/GPU/OCR APIs appear in runtime source or the PE import table.
