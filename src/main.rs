//! Apex Angle Reader — pixel-only high-arc distance display.
//!
//! Loop: capture top-left ROI (xcap/WGC) -> binarize -> template OCR ->
//! extract negative pitch -> inverse formula -> semi-transparent panel.
//! Zero network, zero game access, zero input injection.

#![cfg_attr(windows, windows_subsystem = "windows")]

//! Windows-only binary. On other platforms the crate still builds so the
//! portable formula/OCR tests run on macOS/Linux.

#[cfg(windows)]
mod capture;
#[cfg(windows)]
mod ui;

#[cfg(windows)]
use apex_angle_reader::{formula, ocr, templates};

#[cfg(windows)]
use std::env;
#[cfg(windows)]
use std::ptr;
#[cfg(windows)]
use std::time::{Duration, Instant};

#[cfg(windows)]
use anyhow::{bail, Result};

#[cfg(not(windows))]
fn main() {
    eprintln!(
        "apex-angle-reader: this binary only runs on Windows; use the GitHub Actions artifact."
    );
}

#[cfg(windows)]
/// Only pitch values steeper than this trigger an update; flatter angles map
/// to near-60m distances where the display is meaningless.
const TRIGGER_MIN_ABS_PITCH: f64 = 40.0;

#[cfg(windows)]
fn parse_args() -> Result<(capture::CaptureConfig, usize, i32, i32, f64, bool)> {
    let mut cfg = capture::CaptureConfig::default();
    let mut hz = 60usize;
    let mut left = -1i32;
    let mut top = -1i32;
    let mut offset_m = formula::DISTANCE_OFFSET_M;
    let mut debug = false;
    let mut args = env::args().skip(1);
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--monitor" => {
                cfg.monitor = args
                    .next()
                    .and_then(|v| v.parse().ok())
                    .ok_or_else(|| anyhow::anyhow!("--monitor needs an index"))?;
            }
            "--roi" => {
                let v = args
                    .next()
                    .ok_or_else(|| anyhow::anyhow!("--roi needs x,y,w,h"))?;
                let parts: Vec<usize> = v.split(',').filter_map(|p| p.parse().ok()).collect();
                if parts.len() != 4 {
                    bail!("--roi must be x,y,w,h");
                }
                cfg.roi_x = parts[0] as u32;
                cfg.roi_y = parts[1] as u32;
                cfg.roi_w = parts[2] as u32;
                cfg.roi_h = parts[3] as u32;
            }
            "--hz" => {
                hz = args
                    .next()
                    .and_then(|v| v.parse().ok())
                    .filter(|v| *v >= 10 && *v <= 240)
                    .ok_or_else(|| anyhow::anyhow!("--hz must be 10..240"))?;
            }
            "--top" => {
                top = args
                    .next()
                    .and_then(|v| v.parse().ok())
                    .ok_or_else(|| anyhow::anyhow!("--top needs pixels"))?;
            }
            "--debug" => {
                debug = true;
            }
            "--offset" => {
                offset_m = args
                    .next()
                    .and_then(|v| v.parse().ok())
                    .ok_or_else(|| anyhow::anyhow!("--offset needs metres, e.g. -1.5"))?;
            }
            "--left" => {
                left = args
                    .next()
                    .and_then(|v| v.parse().ok())
                    .ok_or_else(|| anyhow::anyhow!("--left needs pixels"))?;
            }
            "--help" | "-h" => {
                println!(
                    "apex-angle-reader [--monitor N] [--roi x,y,w,h] [--hz 10..240] [--top px]\n\
                     Defaults: monitor 0, roi 0,0,320,120, hz 60.\n\
                     Panel: default center-lower; F10 drag-move (saved to ini),\n\
                     F9 toggles, Ctrl+Shift+Q quits. --left/--top override.\n\
                     --offset METRES: distance compensation (default -1.5)."
                );
                std::process::exit(0);
            }
            other => bail!("unknown argument: {other}"),
        }
    }
    Ok((cfg, hz, left, top, offset_m, debug))
}

#[cfg(windows)]
fn main() -> Result<()> {
    let (cfg, hz, left, top, offset_m, debug) = parse_args()?;
    let templates = templates::build_normalized();
    println!(
        "apex-angle-reader: monitor={} roi={},{},{}x{} hz={} top={}",
        cfg.monitor, cfg.roi_x, cfg.roi_y, cfg.roi_w, cfg.roi_h, hz, top
    );

    let instance = unsafe { ui::GetModuleHandleW(std::ptr::null()) };
    if instance.is_null() {
        bail!("GetModuleHandleW failed");
    }
    let mut win = match ui::Window::new(instance) {
        Ok(w) => w,
        Err(e) => bail!("window init failed: {e}"),
    };
    win.set_left(left);
    win.set_top(top);

    unsafe {
        ui::timeBeginPeriod(1);
    }
    let interval = Duration::from_secs_f64(1.0 / hz as f64);
    let mut last = Instant::now();
    let mut total_frames = 0u64;
    let mut ok_frames = 0u64;
    // 2/3-frame hold: a transient miss keeps the last reading for at most 2
    // frames so brief shadow/particle occlusion does not blank the panel.
    let mut held: Option<(f64, f64, u8)> = None;
    let mut held_frame = 0u64;

    loop {
        // Pump all queued messages (hotkeys, paint, quit).
        let mut quit = false;
        unsafe {
            let mut msg: ui::MSG = std::mem::zeroed();
            while ui::PeekMessageW(&mut msg, ptr::null_mut(), 0, 0, ui::PM_REMOVE) != 0 {
                if msg.message == ui::WM_QUIT {
                    quit = true;
                    break;
                }
                ui::TranslateMessage(&msg);
                ui::DispatchMessageW(&msg);
            }
        }
        if quit {
            break;
        }

        let now = Instant::now();
        let frame_ms = now.duration_since(last).as_secs_f64() * 1000.0;
        last = now;
        total_frames += 1;

        if win.visible() {
            // Capture + OCR + formula. Below |pitch| <= 40 deg the distance is
            // always near 60m (no usable resolution), so keep the overlay on
            // its previous value / "--" instead of wasting an update.
            let mut state = ui::PanelState::default();
            match capture::grab_roi_gray(&cfg) {
                Ok((gray, rgb, w, h)) => {
                    if let Some(result) =
                        ocr::recognize_pitch_multi(&gray, &rgb, w as usize, h as usize, &templates)
                    {
                        held = Some((result.pitch, result.confidence, result.strategy_mask));
                        held_frame = total_frames;
                        // The pitch line always updates so the user sees the
                        // tool is alive; the big distance only appears when
                        // the angle is steep enough to be meaningful.
                        state.pitch = Some(result.pitch);
                        state.confidence = result.confidence;
                        state.strategy_mask = result.strategy_mask;
                        state.debug = debug;
                        state.capture_ok = true;
                        if result.pitch.abs() > TRIGGER_MIN_ABS_PITCH {
                            if let Some(distance) =
                                formula::display_distance_for_pitch(result.pitch, offset_m)
                            {
                                state.distance = Some(distance);
                                ok_frames += 1;
                            }
                        }
                    } else if let Some((p, c, m)) = held {
                        if total_frames - held_frame <= 2 {
                            state.pitch = Some(p);
                            state.confidence = c;
                            state.strategy_mask = m;
                            state.debug = debug;
                            state.capture_ok = true;
                            if p.abs() > TRIGGER_MIN_ABS_PITCH {
                                if let Some(distance) =
                                    formula::display_distance_for_pitch(p, offset_m)
                                {
                                    state.distance = Some(distance);
                                }
                            }
                        } else {
                            held = None;
                        }
                    }
                }
                Err(e) => {
                    eprintln!("capture error: {e:#}");
                }
            }
            if total_frames % (hz as u64 * 5) == 0 {
                let ratio = if total_frames > 0 {
                    ok_frames as f64 / total_frames as f64
                } else {
                    0.0
                };
                println!(
                    "frame={total_frames} ok={ok_frames} ratio={ratio:.2} last_ms={frame_ms:.1}"
                );
            }
            win.set_state(state);
            win.redraw_if_changed();

            // Pace to the requested rate.
            let work = now.elapsed();
            if work < interval {
                std::thread::sleep(interval - work);
            }
        } else {
            // Panel hidden (F9): zero capture, zero OCR, zero CPU. The message
            // pump above still answers F9/Ctrl+Shift+Q immediately.
            std::thread::sleep(Duration::from_millis(50));
        }
    }

    unsafe {
        ui::timeEndPeriod(1);
    }
    Ok(())
}
