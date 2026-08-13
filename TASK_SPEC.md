# Apex Angle Reader — Implementation Contract

## Goal
Windows x64 Rust binary that continuously reads the `ang:` line of the Apex
`cl_showpos` HUD via pixel-only template matching, computes the high-arc throw
distance from the pitch, and shows it in a semi-transparent click-through
overlay. The user reads the distance and decides when to throw; no input from
the user is required (no distance entry).

## Core logic
Forward formula (fixed contract, never replaced by the low branch):
`x = 90 - degrees(asin(y / 60)) / 2` with anchors 30m->75 deg, 60m->45 deg.
Inverse (pitch is negative when looking up): `y = 60 * sin(radians((90 + pitch) * 2))`.
Validation: -75 deg -> 30m, -45 deg -> 60m, -85 deg -> ~10.4m.

## Pipeline
1. `capture.rs`: xcap monitor capture, crop ROI (default 0,0,320x120; monitor 0).
2. `ocr.rs`: binarize (bright + near-neutral, threshold 150 / chroma 64 with
   saturated fallback) -> 8-connected components -> vertical line clustering ->
   aspect-preserving normalization to 12x18 -> SAD/overlap template matching ->
   `ANG:` label validation (A,N,G in order + colon punctuation window) ->
   first numeric token.
3. Fail-closed: the pitch token MUST start with a visible '-'; range
   (-90, 0); any ambiguity yields no update (overlay shows "--").
4. `formula.rs`: inverse formula -> distance.
5. `ui.rs`: Win32 layered window (alpha ~205), click-through, top-left below
   the HUD block (default top 96), large distance digits + small pitch text.
6. `main.rs`: PeekMessage loop + paced capture (default 60 Hz, `--hz` 10..240),
   F9 toggles visibility, Ctrl+Shift+Q quits. CLI: `--monitor N`,
   `--roi x,y,w,h`, `--hz N`, `--top px`.

## Constraints
- Rust only; no OpenCV, no C++ bindings, no OCR SDKs.
- Capture via Windows Graphics Capture (xcap). No network, no WebSocket, no
  process/memory access, no injection, no hooks, no synthetic input.
- Runtime never opens image files; templates are embedded; tests use an
  offline binary fixture derived from the user screenshot (pic.png, not in git).
- macOS/Linux: develop + run portable tests only. Windows build happens
  exclusively in GitHub Actions (push to main).
- The overlay never steals focus and never blocks clicks.

## Deliverables
- Rust crate at repo root (Cargo.toml + src/*), tests, `tools/` extraction
  script, `.github/workflows/build.yml`, README/AGENTS/PROGRESS docs.
- Old C++ code archived under `legacy-cpp/`.
- Rollback transaction under `artifacts/rust-rewrite/`.
