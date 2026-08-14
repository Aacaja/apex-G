//! Offline-derived binary glyph templates for the Apex/Source cl_showpos HUD.
//!
//! The 12x18 "real" templates were extracted by `tools/extract_templates.py`
//! from the user's 8 in-game screenshots (pic/1..8.png, 2560x1440 showing the
//! 2560x1920 render stretched to the display) with vision-verified ground
//! truth, using the same binarization rule as the runtime. The 7x7/5x7
//! fallbacks cover glyphs/positions not sampled (labels, plus, alternates).
//! Runtime never opens image files.

pub const TEMPLATE_W: usize = 12;
pub const TEMPLATE_H: usize = 18;

#[derive(Clone, Copy, Debug)]
pub struct Template {
    pub id: char,
    pub rows: &'static [&'static str],
}

pub const TEMPLATES: &[Template] = &[
    // ---- real glyphs extracted from the user's screenshots (majority vote) --
    Template {
        id: '-',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "....####....",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '.',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            ".....#......",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '0',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "...######...",
            "...##...#...",
            "..####..#...",
            "..##.####...",
            "..##..###...",
            "...######...",
            "....####....",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '1',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "...###......",
            ".....##.....",
            ".....##.....",
            ".....##.....",
            ".....##.....",
            "....####....",
            "...#####....",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '2',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "...######...",
            "...#....#...",
            ".......##...",
            ".....###....",
            "...###......",
            "..######....",
            "...######...",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '3',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "...######...",
            "...#....#...",
            ".......##...",
            ".....####...",
            "........#...",
            "...######...",
            "....####....",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '4',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "..##....#...",
            "..##....#...",
            "...#....#...",
            "...######...",
            ".......##...",
            "........#...",
            "........#...",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '5',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "...#####....",
            "..##........",
            "..######....",
            ".......##...",
            "........#...",
            "...######...",
            "....####....",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '6',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "...#####....",
            "...#........",
            "..##........",
            "..#######...",
            "..##....#...",
            "...######...",
            "....####....",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '7',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "..#######...",
            ".......##...",
            "......##....",
            ".....##.....",
            "...###......",
            "...#........",
            "...#........",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '8',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "...######...",
            "...#....#...",
            "...#...##...",
            "...######...",
            "..##....#...",
            "...######...",
            "....####....",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    Template {
        id: '9',
        rows: &[
            "............",
            "............",
            "............",
            "............",
            "............",
            "...######...",
            "...#....#...",
            "...#...##...",
            "...######...",
            "........#...",
            "...######...",
            "....####....",
            "............",
            "............",
            "............",
            "............",
            "............",
            "............",
        ],
    },
    // ---- label glyphs (7x7, from the original pic.png-derived set) ----
    Template {
        id: 'A',
        rows: &[
            "..####.", ".##.##.", ".##..##", "##...##", "#######", "##....#", ".#....#",
        ],
    },
    Template {
        id: 'N',
        rows: &[
            "###...#", "###...#", "####..#", "##.##.#", "##..###", "##...##", ".#...##",
        ],
    },
    Template {
        id: 'G',
        rows: &[
            ".######", ".#....#", "##.....", "##..###", "##...##", ".######", ".#####.",
        ],
    },
    // ---- fallbacks: 2x2 dot, 4x1 minus, 5x7 digits/plus ----
    Template {
        id: '.',
        rows: &["#.", "##"],
    },
    Template {
        id: '-',
        rows: &["####"],
    },
    Template {
        id: '+',
        rows: &[
            ".....", "..#..", "..#..", "#####", "..#..", "..#..", ".....",
        ],
    },
    Template {
        id: '0',
        rows: &[
            ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###.",
        ],
    },
    Template {
        id: '1',
        rows: &[
            "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###.",
        ],
    },
    Template {
        id: '2',
        rows: &[
            ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####",
        ],
    },
    Template {
        id: '3',
        rows: &[
            ".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###.",
        ],
    },
    Template {
        id: '4',
        rows: &[
            "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#.",
        ],
    },
    Template {
        id: '5',
        rows: &[
            "#####", "#....", "####.", "....#", "....#", "#...#", ".###.",
        ],
    },
    Template {
        id: '6',
        rows: &[
            "..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###.",
        ],
    },
    Template {
        id: '7',
        rows: &[
            "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#...",
        ],
    },
    Template {
        id: '8',
        rows: &[
            ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###.",
        ],
    },
    Template {
        id: '9',
        rows: &[
            ".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##..",
        ],
    },
];

/// A template pre-normalized to the fixed matching grid.
#[derive(Clone, Copy)]
pub struct NormalizedTemplate {
    pub id: char,
    pub grid: [u8; TEMPLATE_W * TEMPLATE_H],
}

/// Normalize a glyph grid onto the fixed grid, preserving aspect ratio and
/// centering.
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

/// Build the pre-normalized template set (call once at startup). Each glyph
/// also gets 4 one-pixel phase variants (shifted copies) so small raster
/// misalignment between the capture and the offline extraction does not drop
/// scores. classify() takes the best score per glyph id, so the variants do
/// not create cross-character confusion.
pub fn build_normalized() -> Vec<NormalizedTemplate> {
    let mut out = Vec::with_capacity(TEMPLATES.len() * 5);
    for t in TEMPLATES {
        let mut grid = [0u8; TEMPLATE_W * TEMPLATE_H];
        normalize_to_grid(t.rows, &mut grid);
        out.push(NormalizedTemplate { id: t.id, grid });
        for (dx, dy) in [(1i32, 0i32), (-1, 0), (0, 1), (0, -1)] {
            let mut shifted = [0u8; TEMPLATE_W * TEMPLATE_H];
            for y in 0..TEMPLATE_H {
                for x in 0..TEMPLATE_W {
                    let sx = x as i32 - dx;
                    let sy = y as i32 - dy;
                    if sx >= 0 && sy >= 0 && sx < TEMPLATE_W as i32 && sy < TEMPLATE_H as i32 {
                        shifted[y * TEMPLATE_W + x] = grid[sy as usize * TEMPLATE_W + sx as usize];
                    }
                }
            }
            out.push(NormalizedTemplate {
                id: t.id,
                grid: shifted,
            });
        }
    }
    out
}
