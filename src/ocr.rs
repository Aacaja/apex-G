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

/// Binarize grayscale+RGB using the same rule the offline template
/// extraction used: bright AND near-neutral (white HUD text with dark
/// shadow), with a saturated-white fallback.
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
            let foreground = hi >= 150 && ((hi - lo) <= 64 || (hi >= 248 && lo >= 190));
            pixels[i] = if foreground { 1 } else { 0 };
        }
    }
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

pub fn classify(g: &Glyph, templates: &[NormalizedTemplate], allowed: &str) -> Option<(char, f64)> {
    let mut norm = [0u8; TEMPLATE_W * TEMPLATE_H];
    normalize_glyph(g, &mut norm);
    let mut best: Option<(char, f64)> = None;
    for t in templates {
        if !allowed.contains(t.id) {
            continue;
        }
        let s = score(&norm, &t.grid);
        if best.map_or(true, |(_, bs)| s > bs) {
            best = Some((t.id, s));
        }
    }
    best
}

/// Cluster glyphs into text lines by vertical center. The tolerance must be
/// generous enough to keep a decimal point (which sits below its digits)
/// together with its token while still separating HUD rows (~20px apart).
fn cluster_lines(glyphs: &[Glyph]) -> Vec<Vec<&Glyph>> {
    let mut sorted: Vec<&Glyph> = glyphs.iter().collect();
    sorted.sort_by_key(|g| 2 * g.y + g.h as i32);
    let mut lines: Vec<Vec<&Glyph>> = Vec::new();
    for g in sorted {
        let cy = 2 * g.y + g.h as i32;
        let mut placed = false;
        for line in lines.iter_mut() {
            let ref_h = line.iter().map(|x| x.h).max().unwrap_or(1) as i32;
            let line_cy =
                line.iter().map(|x| 2 * x.y + x.h as i32).sum::<i32>() / line.len() as i32;
            if (cy - line_cy).abs() <= ref_h.max(4) {
                line.push(g);
                placed = true;
                break;
            }
        }
        if !placed {
            lines.push(vec![g]);
        }
    }
    lines
}

/// Find the `ANG:` label row and return its trailing digit region, or None
/// when no trustworthy ANG row exists. The colon in this HUD font can be two
/// separate small components (one 2x1 bar that looks like a '-' and one 2x2
/// dot), so the check uses a punctuation window right after the G instead of
/// requiring a single '.' component.
fn find_ang_line(
    glyphs: &[Glyph],
    templates: &[NormalizedTemplate],
) -> Option<Vec<(i32, usize, char, f64)>> {
    let lines = cluster_lines(glyphs);
    for line in lines {
        if line.len() < 5 {
            continue;
        }
        let mut chars: Vec<(i32, usize, char, f64)> = line
            .iter()
            .filter_map(|g| {
                classify(g, templates, "ANG-+.0123456789").map(|(c, s)| (g.x, g.w, c, s))
            })
            .collect();
        chars.sort_by_key(|c| c.0);
        if chars.len() < 5 {
            continue;
        }
        // Require A N G in order and closely spaced.
        if chars[0].2 != 'A' || chars[1].2 != 'N' || chars[2].2 != 'G' {
            continue;
        }
        if chars[0].1 < 3 || chars[1].1 < 3 || chars[2].1 < 3 {
            continue;
        }
        let gap1 = chars[1].0 - (chars[0].0 + chars[0].1 as i32);
        let gap2 = chars[2].0 - (chars[1].0 + chars[1].1 as i32);
        if gap1 < 0 || gap1 > 8 || gap2 < 0 || gap2 > 8 {
            continue;
        }
        // Colon: a '.' or bar-like '-' component within the window right
        // after G. The leading minus of the pitch sits much further away.
        let g_right = chars[2].0 + chars[2].1 as i32;
        let window = (chars[2].1 as i32 * 2).max(8);
        let colon_ok = chars.iter().skip(3).any(|c| {
            let d = c.0 - g_right;
            d >= 0 && d <= window && (c.2 == '.' || c.2 == '-')
        });
        if !colon_ok {
            continue;
        }
        // Return only the digit region: everything after the punctuation
        // window. The pitch minus stays included (it is far from G).
        let digits: Vec<(i32, usize, char, f64)> = chars
            .into_iter()
            .skip(3)
            .filter(|c| c.0 - g_right > window || (c.2 != '.' && c.2 != '-'))
            .collect();
        if digits.len() < 3 {
            continue;
        }
        return Some(digits);
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
    let filtered: Vec<Glyph> = glyphs
        .into_iter()
        .filter(|g| g.w >= 2 && g.h >= 1 && g.w <= 96 && g.h <= 96)
        .collect();
    if filtered.len() < 5 {
        return None;
    }
    let mut numeric = find_ang_line(&filtered, templates)?;
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
    let confidence = score_sum / first.len() as f64;
    Some(PitchResult { pitch, confidence })
}
