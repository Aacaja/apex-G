//! Offline-derived binary glyph templates for the Apex/Source cl_showpos HUD.
//!
//! The 7x7 "exact" glyphs were sampled offline from the user's 2560x1440
//! screenshot (pic.png, which shows the 2560x1920 render stretched to the
//! 1440p display, i.e. the exact shape WGC capture sees). These glyphs were
//! previously validated by the legacy C++ port: they recognized `ANG: -52.25`
//! from the same screenshot. The 5x7 fallback covers glyphs not present in
//! that frame (6, +). Runtime never opens image files.

pub const TEMPLATE_W: usize = 12;
pub const TEMPLATE_H: usize = 18;

#[derive(Clone, Copy, Debug)]
pub struct Template {
    pub id: char,
    pub rows: &'static [&'static str],
}

pub const TEMPLATES: &[Template] = &[
    // Label glyphs (uppercase A/N/G from the HUD).
    Template { id: 'A', rows: &["..####.", ".##.##.", ".##..##", "##...##", "#######", "##....#", ".#....#"] },
    Template { id: 'N', rows: &["###...#", "###...#", "####..#", "##.##.#", "##..###", "##...##", ".#...##"] },
    Template { id: 'G', rows: &[".######", ".#....#", "##.....", "##..###", "##...##", ".######", ".#####."] },
    // Sign and punctuation.
    Template { id: '-', rows: &["####"] },
    Template { id: '.', rows: &["#.", "##"] },
    // Digits sampled from the screenshot (two variants for 2 and 5).
    Template { id: '0', rows: &["######", "##..##", "###..#", "#.####", "#..###", "######", "#####."] },
    Template { id: '1', rows: &["###..", "..##.", "..##.", "..##.", "..##.", "..##.", "#####"] },
    Template { id: '2', rows: &[".######", ".#...##", ".....##", "...####", ".###...", "#######", ".######"] },
    Template { id: '2', rows: &["#####.", "##..##", "#....#", "...###", "####..", "##....", "######"] },
    Template { id: '3', rows: &["#####.", "##..##", "#....#", "..####", "....##", "#....#", "######"] },
    Template { id: '4', rows: &["#....#", "#....#", "#....#", "######", ".#####", ".....#", ".....#"] },
    Template { id: '5', rows: &[".######", "##.....", "######.", ".....##", ".#....#", ".######", ".#####."] },
    Template { id: '5', rows: &["#####.", "#.....", "#.....", "######", ".....#", "#....#", "######"] },
    Template { id: '7', rows: &["######", "....##", "....##", "..###.", ".##...", "##....", "#....."] },
    Template { id: '8', rows: &["#####.", "##..##", "##...#", "####.#", "#..###", "#...##", "######"] },
    Template { id: '9', rows: &[".######", "##....#", ".##..##", ".######", "......#", ".######", ".#####."] },
    // 5x7 fallback for glyphs not present in the reference frame (6, +) and
    // a generic fallback for all numeric glyphs.
    Template { id: '6', rows: &["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."] },
    Template { id: '+', rows: &[".....", "..#..", "..#..", "#####", "..#..", "..#..", "....."] },
    Template { id: '0', rows: &[".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."] },
    Template { id: '1', rows: &["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."] },
    Template { id: '2', rows: &[".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"] },
    Template { id: '3', rows: &[".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###."] },
    Template { id: '4', rows: &["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."] },
    Template { id: '5', rows: &["#####", "#....", "####.", "....#", "....#", "#...#", ".###."] },
    Template { id: '6', rows: &["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."] },
    Template { id: '7', rows: &["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."] },
    Template { id: '8', rows: &[".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."] },
    Template { id: '9', rows: &[".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."] },
    Template { id: '-', rows: &[".....", ".....", ".....", "#####", ".....", ".....", "....."] },
    Template { id: '.', rows: &[".....", ".....", ".....", ".....", ".###.", ".###.", "....."] },
];

/// A template pre-normalized to the fixed matching grid.
#[derive(Clone, Copy)]
pub struct NormalizedTemplate {
    pub id: char,
    pub grid: [u8; TEMPLATE_W * TEMPLATE_H],
}

/// Normalize a glyph grid onto the fixed grid, preserving aspect ratio and
/// centering (same algorithm the legacy C++ port used successfully).
pub fn normalize_to_grid(rows: &[&str], out: &mut [u8; TEMPLATE_W * TEMPLATE_H]) {
    out.fill(0);
    let gw = rows.iter().map(|r| r.len()).max().unwrap_or(0);
    let gh = rows.len();
    if gw == 0 || gh == 0 {
        return;
    }
    let scale = ((TEMPLATE_W as f64 / gw as f64).min(TEMPLATE_H as f64 / gh as f64)).min(1.0);
    let sw = (gw as f64 * scale).round().max(1.0) as usize;
    let sh = (gh as f64 * scale).round().max(1.0) as usize;
    let ox = (TEMPLATE_W - sw) / 2;
    let oy = (TEMPLATE_H - sh) / 2;
    for y in 0..sh {
        let sy = (y as f64 / scale).min((gh - 1) as f64) as usize;
        let row = rows[sy];
        for x in 0..sw {
            let sx = (x as f64 / scale).min((gw - 1) as f64) as usize;
            if row.as_bytes().get(sx).copied() == Some(b'#') {
                out[(oy + y) * TEMPLATE_W + (ox + x)] = 1;
            }
        }
    }
}

/// Build the pre-normalized template set (call once at startup).
pub fn build_normalized() -> Vec<NormalizedTemplate> {
    let mut out = Vec::with_capacity(TEMPLATES.len());
    for t in TEMPLATES {
        let mut grid = [0u8; TEMPLATE_W * TEMPLATE_H];
        normalize_to_grid(t.rows, &mut grid);
        out.push(NormalizedTemplate { id: t.id, grid });
    }
    out
}
