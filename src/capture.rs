//! Screen capture: xcap WGC grab of a small top-left ROI -> grayscale.

use anyhow::{anyhow, Context, Result};

pub struct CaptureConfig {
    pub monitor: usize,
    pub roi_x: u32,
    pub roi_y: u32,
    pub roi_w: u32,
    pub roi_h: u32,
}

impl Default for CaptureConfig {
    fn default() -> Self {
        // Defaults measured for the user's setup (2560x1440 display, HUD block
        // at y 36..76, x 0..~240): a 320x120 ROI covers the whole cl_showpos
        // block with margin.
        Self {
            monitor: 0,
            roi_x: 0,
            roi_y: 0,
            roi_w: 320,
            roi_h: 120,
        }
    }
}

/// Grab the configured ROI of the chosen monitor (Windows/xcap only).
/// Returns (gray_pixels, rgb_pixels, width, height). RGB is 3 bytes/pixel and
/// lets the binarizer enforce the near-neutral rule like the offline
/// template extraction did.
#[cfg(windows)]
pub fn grab_roi_gray(cfg: &CaptureConfig) -> Result<(Vec<u8>, Vec<u8>, u32, u32)> {
    let monitors = xcap::Monitor::all().context("enumerate monitors")?;
    let monitor = monitors
        .get(cfg.monitor)
        .ok_or_else(|| anyhow!("monitor index {} not found", cfg.monitor))?;
    let image = monitor.capture_image().context("capture monitor frame")?;
    let (img_w, img_h) = image.dimensions();

    // Clamp the ROI to the captured frame.
    let roi_x = cfg.roi_x.min(img_w);
    let roi_y = cfg.roi_y.min(img_h);
    let w = cfg.roi_w.min(img_w - roi_x) as usize;
    let h = cfg.roi_h.min(img_h - roi_y) as usize;
    if w == 0 || h == 0 {
        return Err(anyhow!("empty ROI after clamping"));
    }

    let mut gray = vec![0u8; w * h];
    let mut rgb = vec![0u8; w * h * 3];
    for y in 0..h {
        for x in 0..w {
            let px = image.get_pixel(roi_x + x as u32, roi_y + y as u32);
            let [r, g, b, _a] = px.0;
            let i = y * w + x;
            // Rec.601 luma.
            gray[i] = ((299 * r as u32 + 587 * g as u32 + 114 * b as u32) / 1000) as u8;
            let j = i * 3;
            rgb[j] = r;
            rgb[j + 1] = g;
            rgb[j + 2] = b;
        }
    }
    Ok((gray, rgb, w as u32, h as u32))
}

/// Non-Windows stub so the crate still compiles (and tests run) on macOS/Linux.
#[cfg(not(windows))]
pub fn grab_roi_gray(_cfg: &CaptureConfig) -> Result<(Vec<u8>, Vec<u8>, u32, u32)> {
    Err(anyhow!("screen capture is only available on Windows"))
}
