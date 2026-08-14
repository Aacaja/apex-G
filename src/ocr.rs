//! Pure-Rust pixel OCR for the cl_showpos HUD.
//!
//! Pipeline: binarize (bright + near-neutral) -> 8-connected components ->
//! line clustering -> aspect-preserving normalized template matching ->
//! find the `ANG:` label row -> parse the first (pitch) numeric token.
//!
//! Fail-closed rules:
//! - Only the `ANG:` row is ever parsed (pos:/vel: rows are ignored).
//! - The pitch token MUST start with a visible '-' (looking up is negative).
//!   A missing/merged sign yields no result, never a mirrored distance.
//! - Out-of-range values and ambiguous rows yield no result.

use crate::templates::{NormalizedTemplate, TEMPLATE_H, TEMPLATE_W};

#[derive(Clone, Debug)]
pub struct BinaryImage {
    pub width: usize,
    pub height: usize,
    pub pixels: Vec<u8>, // 0 or 1
}

/// Binarize grayscale+RGB: bright AND near-neutral (white HUD text), with a
/// saturated-white fallback. A drop-shadow test was tried but it eroded the
/// interior strokes of small glyphs (e.g. the bar of '4'), so the binarizer
/// stays a plain threshold; background noise is rejected later by the ANG
/// anchor + per-glyph score thresholds.
pub fn binarize(gray: &[u8], rgb: &[u8], width: usize, height: usize) -> BinaryImage {
    let mut pixels = vec![0u8; width * height];
    for y in 0..height {
        for x in 0..width {
            let i = y * width + x;
            let j = i * 3;
            let r = rgb[j] as i32;
            let g = rgb[j + 1] as i32;
            let b = rgb[j + 2] as i32;
            let hi = r.max(g).max(b);
            let lo = r.min(g).min(b);
            // Saturated-white fallback plus a tinted-bright rule so strokes
            // blended onto colored scenery (e.g. a red wall dimming the
            // white glyphs) are not dropped: hi>=155 with chroma<=100.
            let foreground = hi >= 150
                && ((hi - lo) <= 64 || (hi >= 248 && lo >= 190) || (hi >= 155 && (hi - lo) <= 100));
            pixels[i] = if foreground { 1 } else { 0 };
        }
    }
    let _ = gray;
    BinaryImage {
        width,
        height,
        pixels,
    }
}

#[derive(Clone, Debug)]
pub struct Glyph {
    pub x: i32,
    pub y: i32,
    pub w: usize,
    pub h: usize,
    pub pixels: Vec<u8>, // w*h
}

/// Local-contrast binarizer: text = pixels clearly brighter than their 5x5
/// neighbourhood (integral image). Global illumination does not matter, so
/// bright snow scenes and dark caves both work as long as the white glyph
/// stays locally brighter than its surroundings.
pub fn binarize_local_contrast(
    gray: &[u8],
    rgb: &[u8],
    width: usize,
    height: usize,
) -> BinaryImage {
    const RADIUS: usize = 2; // 5x5 window
    const CONTRAST: i32 = 32; // min local brightness delta
    const MIN_GRAY: i32 = 70; // still needs some light
    let mut pixels = vec![0u8; width * height];
    // Integral image of gray.
    let mut intg = vec![0i32; (width + 1) * (height + 1)];
    for y in 0..height {
        let mut row = 0i32;
        for x in 0..width {
            row += gray[y * width + x] as i32;
            intg[(y + 1) * (width + 1) + (x + 1)] = intg[y * (width + 1) + (x + 1)] + row;
        }
    }
    let sum = |x0: usize, y0: usize, x1: usize, y1: usize| -> i32 {
        intg[(y1) * (width + 1) + (x1)]
            - intg[(y0) * (width + 1) + (x1)]
            - intg[(y1) * (width + 1) + (x0)]
            + intg[(y0) * (width + 1) + (x0)]
    };
    let area = ((2 * RADIUS + 1) * (2 * RADIUS + 1)) as i32;
    for y in 0..height {
        for x in 0..width {
            let x0 = x.saturating_sub(RADIUS);
            let y0 = y.saturating_sub(RADIUS);
            let x1 = (x + RADIUS).min(width - 1) + 1;
            let y1 = (y + RADIUS).min(height - 1) + 1;
            let mean = sum(x0, y0, x1, y1) / ((x1 - x0) * (y1 - y0)) as i32;
            let i = y * width + x;
            let g = gray[i] as i32;
            if g >= MIN_GRAY && g - mean >= CONTRAST {
                // Keep the near-neutral/white expectation too: reject strongly
                // saturated background edges.
                let j = i * 3;
                let r = rgb[j] as i32;
                let g2 = rgb[j + 1] as i32;
                let b = rgb[j + 2] as i32;
                let hi = r.max(g2).max(b);
                let lo = r.min(g2).min(b);
                if hi - lo <= 110 {
                    pixels[i] = 1;
                }
            }
        }
    }
    BinaryImage {
        width,
        height,
        pixels,
    }
}

/// Shadow-aware binarizer: white glyphs carry a dark drop shadow. Take every
/// bright pixel (>=120), grow it into connected components, and keep an
/// entire component only when its outer 1px border touches a dark pixel
/// (luma <=90). Background surfaces never have that shadow ring.
pub fn binarize_shadow(gray: &[u8], rgb: &[u8], width: usize, height: usize) -> BinaryImage {
    const BRIGHT: u8 = 140;
    const DARK: u8 = 92;
    const MIN_SHADOW_PX: usize = 3; // dark pixels on the component border
    let mut bright = vec![0u8; width * height];
    for y in 0..height {
        for x in 0..width {
            let i = y * width + x;
            let g = gray[i];
            if g >= BRIGHT {
                // Keep only light/near-neutral or mildly tinted pixels.
                let j = i * 3;
                let r = rgb[j] as i32;
                let g2 = rgb[j + 1] as i32;
                let b = rgb[j + 2] as i32;
                let hi = r.max(g2).max(b);
                let lo = r.min(g2).min(b);
                if hi - lo <= 100 {
                    bright[i] = 1;
                }
            }
        }
    }
    // Connected components (8-way) of the bright mask.
    let mut visited = vec![false; width * height];
    let mut out = vec![0u8; width * height];
    let mut stack: Vec<(i32, i32)> = Vec::new();
    for sy in 0..height as i32 {
        for sx in 0..width as i32 {
            let si = sy as usize * width + sx as usize;
            if bright[si] == 0 || visited[si] {
                continue;
            }
            let mut cells: Vec<(i32, i32)> = Vec::new();
            stack.clear();
            stack.push((sx, sy));
            visited[si] = true;
            while let Some((cx, cy)) = stack.pop() {
                cells.push((cx, cy));
                for dy in -1..=1 {
                    for dx in -1..=1 {
                        if dx == 0 && dy == 0 {
                            continue;
                        }
                        let nx = cx + dx;
                        let ny = cy + dy;
                        if nx < 0 || ny < 0 || nx >= width as i32 || ny >= height as i32 {
                            continue;
                        }
                        let ni = ny as usize * width + nx as usize;
                        if bright[ni] != 0 && !visited[ni] {
                            visited[ni] = true;
                            stack.push((nx, ny));
                        }
                    }
                }
            }
            // Count dark pixels on the component's outer border; a genuine
            // shadow ring yields several of them.
            let mut shadow_px = 0usize;
            for &(cx, cy) in &cells {
                for dy in -1..=1 {
                    for dx in -1..=1 {
                        let nx = cx + dx;
                        let ny = cy + dy;
                        if nx < 0 || ny < 0 || nx >= width as i32 || ny >= height as i32 {
                            continue;
                        }
                        let ni = ny as usize * width + nx as usize;
                        if bright[ni] == 0 && gray[ni] <= DARK {
                            shadow_px += 1;
                        }
                    }
                }
            }
            if shadow_px >= MIN_SHADOW_PX {
                for &(cx, cy) in &cells {
                    out[cy as usize * width + cx as usize] = 1;
                }
            }
        }
    }
    BinaryImage {
        width,
        height,
        pixels: out,
    }
}

/// 8-connected component labeling.
pub fn segment(bin: &BinaryImage) -> Vec<Glyph> {
    let w = bin.width;
    let h = bin.height;
    let mut visited = vec![false; w * h];
    let mut out = Vec::new();
    let mut stack: Vec<(i32, i32)> = Vec::new();
    for sy in 0..h as i32 {
        for sx in 0..w as i32 {
            let si = sy as usize * w + sx as usize;
            if bin.pixels[si] == 0 || visited[si] {
                continue;
            }
            let mut min_x = sx;
            let mut min_y = sy;
            let mut max_x = sx;
            let mut max_y = sy;
            let mut cells: Vec<(i32, i32)> = Vec::new();
            stack.clear();
            stack.push((sx, sy));
            visited[si] = true;
            while let Some((cx, cy)) = stack.pop() {
                cells.push((cx, cy));
                min_x = min_x.min(cx);
                min_y = min_y.min(cy);
                max_x = max_x.max(cx);
                max_y = max_y.max(cy);
                for dy in -1..=1 {
                    for dx in -1..=1 {
                        if dx == 0 && dy == 0 {
                            continue;
                        }
                        let nx = cx + dx;
                        let ny = cy + dy;
                        if nx < 0 || ny < 0 || nx >= w as i32 || ny >= h as i32 {
                            continue;
                        }
                        let ni = ny as usize * w + nx as usize;
                        if bin.pixels[ni] != 0 && !visited[ni] {
                            visited[ni] = true;
                            stack.push((nx, ny));
                        }
                    }
                }
            }
            let gw = (max_x - min_x + 1) as usize;
            let gh = (max_y - min_y + 1) as usize;
            let mut gp = vec![0u8; gw * gh];
            for (cx, cy) in cells {
                gp[(cy - min_y) as usize * gw + (cx - min_x) as usize] = 1;
            }
            out.push(Glyph {
                x: min_x,
                y: min_y,
                w: gw,
                h: gh,
                pixels: gp,
            });
        }
    }
    out.sort_by_key(|g| (g.y, g.x));
    out
}

/// Aspect-preserving normalization onto the fixed template grid.
pub fn normalize_glyph(g: &Glyph, out: &mut [u8; TEMPLATE_W * TEMPLATE_H]) {
    out.fill(0);
    let gw = g.w as f64;
    let gh = g.h as f64;
    if gw <= 0.0 || gh <= 0.0 {
        return;
    }
    let scale = ((TEMPLATE_W as f64 / gw).min(TEMPLATE_H as f64 / gh)).min(1.0);
    let sw = (gw * scale).round().max(1.0) as usize;
    let sh = (gh * scale).round().max(1.0) as usize;
    let ox = (TEMPLATE_W - sw) / 2;
    let oy = (TEMPLATE_H - sh) / 2;
    for y in 0..sh {
        let sy = ((y as f64 / scale).floor() as usize).min(g.h - 1);
        for x in 0..sw {
            let sx = ((x as f64 / scale).floor() as usize).min(g.w - 1);
            if g.pixels[sy * g.w + sx] != 0 {
                out[(oy + y) * TEMPLATE_W + (ox + x)] = 1;
            }
        }
    }
}

/// SAD-style combined score: foreground overlap + full-grid equality.
fn score(a: &[u8; TEMPLATE_W * TEMPLATE_H], b: &[u8; TEMPLATE_W * TEMPLATE_H]) -> f64 {
    let mut inter = 0usize;
    let mut ca = 0usize;
    let mut cb = 0usize;
    let mut same = 0usize;
    for i in 0..TEMPLATE_W * TEMPLATE_H {
        ca += a[i] as usize;
        cb += b[i] as usize;
        if a[i] != 0 && b[i] != 0 {
            inter += 1;
        }
        if a[i] == b[i] {
            same += 1;
        }
    }
    let overlap = if ca.max(cb) == 0 {
        0.0
    } else {
        inter as f64 / ca.max(cb) as f64
    };
    let equal = same as f64 / (TEMPLATE_W * TEMPLATE_H) as f64;
    0.76 * overlap + 0.24 * equal
}

/// Classify a glyph. Returns (char, best_score, margin) where margin is the
/// difference between the best and the second-best glyph ids (per-id best
/// across phase variants). A small margin means the match is ambiguous.
pub fn classify(
    g: &Glyph,
    templates: &[NormalizedTemplate],
    allowed: &str,
) -> Option<(char, f64, f64)> {
    let mut norm = [0u8; TEMPLATE_W * TEMPLATE_H];
    normalize_glyph(g, &mut norm);
    // Per-id best score (phase variants share the same id).
    let mut per_id: Vec<(char, f64)> = Vec::new();
    for t in templates {
        if !allowed.contains(t.id) {
            continue;
        }
        let s = score(&norm, &t.grid);
        if let Some(e) = per_id.iter_mut().find(|(id, _)| *id == t.id) {
            if s > e.1 {
                e.1 = s;
            }
        } else {
            per_id.push((t.id, s));
        }
    }
    per_id.sort_by(|a, b| b.1.partial_cmp(&a.1).unwrap_or(std::cmp::Ordering::Equal));
    let best = per_id.first()?;
    let second = per_id.get(1).map(|e| e.1).unwrap_or(0.0);
    Some((best.0, best.1, best.1 - second))
}

/// Find the `ANG:` label row and return its trailing digit region, or None
/// when no trustworthy ANG row exists.
///
/// Strategy: scan ALL components (sorted by x) for the A-N-G label sequence
/// (vertical centers within one glyph height, gaps within bounds), then
/// collect every component whose center lies on the same line as the label.
/// This is far more robust than clustering the whole frame into lines, which
/// merges HUD rows when background noise is present. The colon may be two
/// separate small components (a 2x1 bar and a 2x2 dot); the pitch minus sits
/// well beyond the colon window and is kept.
fn find_ang_line(
    glyphs: &[Glyph],
    templates: &[NormalizedTemplate],
) -> Option<(Vec<(i32, usize, char, f64)>, f64)> {
    // Label candidates sorted by x.
    let mut labels: Vec<(&Glyph, char, f64)> = glyphs
        .iter()
        .filter(|g| g.w >= 3 && g.h >= 3 && g.w <= 32 && g.h <= 32)
        .filter_map(|g| classify(g, templates, "ANG").map(|(c, s, _)| (g, c, s)))
        .collect();
    labels.sort_by_key(|(g, _, _)| g.x);

    for i in 0..labels.len().saturating_sub(2) {
        let (a, ca, sa) = labels[i];
        if ca != 'A' {
            continue;
        }
        for j in (i + 1)..labels.len().saturating_sub(1) {
            let (n, cn, sn) = labels[j];
            if cn != 'N' {
                continue;
            }
            let h_ref = a.h.max(n.h) as i32;
            if (2 * n.y + n.h as i32 - (2 * a.y + a.h as i32)).abs() > h_ref {
                continue; // same x may hold glyphs from other HUD rows
            }
            let gap_n = n.x - (a.x + a.w as i32);
            if gap_n < 0 || gap_n > (h_ref * 2).max(6) {
                continue;
            }
            for k in (j + 1)..labels.len() {
                let (g, cg, sg) = labels[k];
                if cg != 'G' {
                    continue;
                }
                let h_max = h_ref.max(g.h as i32);
                if (2 * g.y + g.h as i32 - (2 * a.y + a.h as i32)).abs() > h_max {
                    continue;
                }
                let gap_g = g.x - (n.x + n.w as i32);
                if gap_g < 0 || gap_g > (h_max * 2).max(6) {
                    continue;
                }
                // Colon: '.' or bar-like '-' component within the window
                // right after G. The pitch minus sits far beyond it.
                let g_right = g.x + g.w as i32;
                let window = (h_max * 2).max(8);
                let anchor_cy = 2 * a.y + a.h as i32;
                let colon_ok = glyphs.iter().any(|c| {
                    let d = c.x - g_right;
                    d >= 0
                        && d <= window
                        && (2 * c.y + c.h as i32 - anchor_cy).abs() <= h_max
                        && classify(c, templates, "-.")
                            .map_or(false, |(ch, sc, _)| (ch == '.' || ch == '-') && sc >= 0.4)
                });
                if !colon_ok {
                    continue;
                }

                // Collect every component on the ANG baseline, then return
                // the digit region after the colon window. The pitch minus is
                // far enough to survive.
                let tol = h_max.max(6);
                let mut chars: Vec<(i32, usize, char, f64, f64)> = glyphs
                    .iter()
                    .filter(|c| {
                        (2 * c.y + c.h as i32 - anchor_cy).abs() <= tol
                            && c.w >= 1
                            && c.h >= 1
                            && c.w <= 48
                            && c.h <= 48
                    })
                    .filter_map(|c| {
                        classify(c, templates, "-+.0123456789")
                            .map(|(ch, s, gap)| (c.x, c.w, ch, s, gap))
                    })
                    .filter(|(_, _, _, s, gap)| *s >= 0.55 && *gap >= 0.03)
                    .collect();
                chars.sort_by_key(|c| c.0);
                let digits: Vec<(i32, usize, char, f64)> = chars
                    .into_iter()
                    .map(|(x, w, ch, s, _gap)| (x, w, ch, s))
                    .filter(|c| c.0 - g_right > window)
                    .collect();
                if digits.len() < 3 {
                    continue;
                }
                return Some((digits, (sa + sn + sg) / 3.0));
            }
        }
    }
    None
}

pub struct PitchResult {
    pub pitch: f64,
    pub confidence: f64,
}

/// Recognize the pitch (first numeric token of the ANG row).
/// Returns None on any ambiguity; never returns an unsigned nonzero pitch.
pub fn recognize_pitch(bin: &BinaryImage, templates: &[NormalizedTemplate]) -> Option<PitchResult> {
    let glyphs = segment(bin);
    // Keep punctuation too: the minus is 4x1 and the colon 2x2 in this HUD
    // font. 1x1 noise is dropped; classify() gates the rest.
    // Keep 1x1 components: the stretched HUD font renders '.' as a single
    // pixel. classify() gates noise with a score threshold later.
    let filtered: Vec<Glyph> = glyphs
        .into_iter()
        .filter(|g| g.w >= 1 && g.h >= 1 && g.w <= 96 && g.h <= 96)
        .collect();
    if filtered.len() < 5 {
        return None;
    }
    let (mut numeric, label_confidence) = find_ang_line(&filtered, templates)?;
    // Keep only numeric glyphs; a stray 'A'-looking component is ignored.
    numeric.retain(|c| "-+.0123456789".contains(c.2));

    let max_h = numeric.iter().map(|c| c.1).max().unwrap_or(7) as i32;
    let token_gap = (max_h * 5 / 4).max(4);

    // Split tokens by horizontal gap.
    let mut tokens: Vec<Vec<(i32, usize, char, f64)>> = Vec::new();
    for c in numeric {
        if let Some(last) = tokens.last_mut() {
            let prev = last.last().unwrap();
            let gap = c.0 - (prev.0 + prev.1 as i32);
            if gap <= token_gap {
                last.push(c);
                continue;
            }
        }
        tokens.push(vec![c]);
    }

    let first = tokens.first()?;
    if first.is_empty() {
        return None;
    }
    // The pitch MUST start with a visible minus sign (looking up = negative).
    if first[0].2 != '-' {
        return None;
    }
    // Exactly one '-' allowed at the start; then digits with at most one '.'.
    let mut text = String::new();
    let mut dots = 0usize;
    let mut score_sum = 0.0f64;
    for (i, c) in first.iter().enumerate() {
        if i == 0 {
            if c.2 != '-' {
                return None;
            }
        } else if c.2 == '.' {
            dots += 1;
            if dots > 1 {
                return None;
            }
        } else if !c.2.is_ascii_digit() {
            return None;
        }
        text.push(c.2);
        score_sum += c.3;
    }
    if text.len() < 3 {
        return None;
    }
    let pitch: f64 = text.parse().ok()?;
    if !(-90.0..0.0).contains(&pitch) {
        return None;
    }
    let confidence = 0.5 * label_confidence + 0.5 * score_sum / first.len() as f64;
    Some(PitchResult { pitch, confidence })
}

// Strategy masks for the multi-binarizer pipeline.
pub const STRATEGY_GLOBAL: u8 = 1;
pub const STRATEGY_CONTRAST: u8 = 2;
pub const STRATEGY_SHADOW: u8 = 4;

pub struct MultiPitchResult {
    pub pitch: f64,
    pub confidence: f64,
    pub strategy_mask: u8,
}

/// Run the three binarizers (global threshold, local contrast, shadow-aware)
/// over the same frame and combine the results:
/// - the calibrated global strategy wins whenever it succeeds (others only
///   corroborate and get OR-ed into the mask);
/// - otherwise the best of contrast/shadow is used;
/// - disagreeing values are never mixed.
pub fn recognize_pitch_multi(
    gray: &[u8],
    rgb: &[u8],
    width: usize,
    height: usize,
    templates: &[NormalizedTemplate],
) -> Option<MultiPitchResult> {
    let bins = [
        (binarize(gray, rgb, width, height), STRATEGY_GLOBAL),
        (
            binarize_local_contrast(gray, rgb, width, height),
            STRATEGY_CONTRAST,
        ),
        (binarize_shadow(gray, rgb, width, height), STRATEGY_SHADOW),
    ];
    let mut global: Option<(f64, f64)> = None;
    let mut others: Vec<(f64, f64, u8)> = Vec::new();
    for (bin, mask) in bins {
        if let Some(r) = recognize_pitch(&bin, templates) {
            if mask == STRATEGY_GLOBAL {
                global = Some((r.pitch, r.confidence));
            } else {
                others.push((r.pitch, r.confidence, mask));
            }
        }
    }
    if let Some((p, c)) = global {
        let corr = others
            .iter()
            .find(|(op, _, _)| (*op - p).abs() < 0.02)
            .map(|(_, _, m)| *m)
            .unwrap_or(0);
        return Some(MultiPitchResult {
            pitch: p,
            confidence: c,
            strategy_mask: STRATEGY_GLOBAL | corr,
        });
    }
    others.sort_by(|a, b| b.1.partial_cmp(&a.1).unwrap_or(std::cmp::Ordering::Equal));
    others.first().map(|(p, c, m)| MultiPitchResult {
        pitch: *p,
        confidence: *c,
        strategy_mask: *m,
    })
}
