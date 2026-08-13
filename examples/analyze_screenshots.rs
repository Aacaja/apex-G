//! Offline accuracy probe with pipeline debugging. Cross-platform (lib only).

use apex_angle_reader::{formula, ocr, templates};

fn main() {
    // (name, expected_pitch, expected_trigger). pic8 shows -4.08 which is
    // below the |pitch| > 40 trigger, so NONE is the correct outcome.
    let truth = [
        ("1", -43.73, true),
        ("2", -10.07, false),
        ("3", -45.63, true),
        ("4", -89.00, true),
        ("5", -89.00, true),
        ("6", -89.00, true),
        ("7", -34.12, false),
        ("8", -4.08, false),
    ];
    let templates = templates::build_normalized();
    let mut ok = 0usize;
    for (name, expected, triggered) in truth {
        let path = format!("build/pic-raw/{name}.rgb");
        let data = std::fs::read(&path).unwrap();
        let (w, h) = (320usize, 120usize);
        let mut gray = vec![0u8; w * h];
        for y in 0..h {
            for x in 0..w {
                let j = (y * w + x) * 3;
                let r = data[j] as u32;
                let g = data[j + 1] as u32;
                let b = data[j + 2] as u32;
                gray[y * w + x] = ((299 * r + 587 * g + 114 * b) / 1000) as u8;
            }
        }
        let bin = ocr::binarize(&gray, &data, w, h);
        let glyphs = ocr::segment(&bin);
        let filtered: Vec<_> = glyphs
            .iter()
            .filter(|g| g.w >= 1 && g.h >= 1 && g.w <= 96 && g.h <= 96)
            .collect();

        // Mirror find_ang_line's anchor + digit collection for diagnostics.
        let mut labels: Vec<_> = filtered
            .iter()
            .filter(|g| g.w >= 3 && g.h >= 3 && g.w <= 32 && g.h <= 32)
            .filter_map(|g| ocr::classify(g, &templates, "ANG").map(|(c, s)| (g, c, s)))
            .collect();
        labels.sort_by_key(|(g, _, _)| g.x);
        let mut anchor: Option<(i32, i32, i32, i32)> = None;
        for i in 0..labels.len().saturating_sub(2) {
            let (a, ca, _) = labels[i];
            if ca != 'A' {
                continue;
            }
            for j in (i + 1)..labels.len().saturating_sub(1) {
                let (n, cn, _) = labels[j];
                if cn != 'N' {
                    continue;
                }
                let h_ref = a.h.max(n.h) as i32;
                if (2 * n.y + n.h as i32 - (2 * a.y + a.h as i32)).abs() > h_ref {
                    continue;
                }
                let gap_n = n.x - (a.x + a.w as i32);
                if gap_n < 0 || gap_n > (h_ref * 2).max(6) {
                    continue;
                }
                for k in (j + 1)..labels.len() {
                    let (g, cg, _) = labels[k];
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
                    anchor = Some((
                        2 * a.y + a.h as i32,
                        g.x + g.w as i32,
                        (h_max * 2).max(8),
                        h_max,
                    ));
                    break;
                }
                if anchor.is_some() {
                    break;
                }
            }
            if anchor.is_some() {
                break;
            }
        }
        if let Some((anchor_cy, g_right, window, h_max)) = anchor {
            let tol = h_max.max(6);
            let mut chars: Vec<(i32, usize, char, f64)> = filtered
                .iter()
                .filter(|c| {
                    (2 * c.y + c.h as i32 - anchor_cy).abs() <= tol
                        && c.w >= 1
                        && c.h >= 1
                        && c.w <= 48
                        && c.h <= 48
                })
                .filter_map(|c| {
                    ocr::classify(c, &templates, "-+.0123456789").map(|(ch, s)| (c.x, c.w, ch, s))
                })
                .filter(|(_, _, _, s)| *s >= 0.55)
                .collect();
            chars.sort_by_key(|c| c.0);
            let digits: Vec<_> = chars
                .into_iter()
                .filter(|c| c.0 - g_right > window)
                .collect();
            println!(
                "  digits({}): {:?}",
                digits.len(),
                digits.iter().map(|c| (c.0, c.1, c.2)).collect::<Vec<_>>()
            );
            let max_h = digits.iter().map(|c| c.1).max().unwrap_or(7) as i32;
            let token_gap = (max_h * 5 / 4).max(4);
            let mut tokens: Vec<Vec<(i32, usize, char, f64)>> = Vec::new();
            for c in digits {
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
            for (ti, t) in tokens.iter().enumerate() {
                let text: String = t.iter().map(|c| c.2).collect();
                println!("    token{ti}: '{text}'");
            }
        } else {
            println!("  anchor: none");
        }

        let got = ocr::recognize_pitch(&bin, &templates);
        let match_str = match got {
            Some(r) => {
                let d = formula::distance_for_pitch(r.pitch)
                    .map(|d| format!("{d:.1} m"))
                    .unwrap_or_else(|| "n/a".into());
                let diff = (r.pitch - expected).abs();
                let good = diff < 0.05;
                if good {
                    ok += 1;
                }
                format!(
                    "pitch={:.2} dist={} conf={:.3} diff={:.2} {}",
                    r.pitch,
                    d,
                    r.confidence,
                    diff,
                    if good { "OK" } else { "MISMATCH" }
                )
            }
            None => {
                if !triggered {
                    format!("NONE (correct: |{expected}| below trigger)")
                } else {
                    format!("NONE (expected {expected})")
                }
            }
        };
        println!("pic{name}: truth={expected:>7} -> {match_str}");
    }
    println!("accuracy {ok}/{}", truth.len());
}
