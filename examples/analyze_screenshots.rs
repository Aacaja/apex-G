//! Offline accuracy probe: run the multi-strategy pipeline on the user's 8
//! real screenshots (build/pic-raw/N.rgb) and print per-strategy results.
//! Cross-platform (lib only).

use apex_angle_reader::{formula, ocr, templates};

fn main() {
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
        // Per-strategy results.
        let mut strategies = Vec::new();
        for (bin, label) in [
            (ocr::binarize(&gray, &data, w, h), "A"),
            (ocr::binarize_local_contrast(&gray, &data, w, h), "B"),
            (ocr::binarize_shadow(&gray, &data, w, h), "C"),
        ] {
            let r = ocr::recognize_pitch(&bin, &templates);
            strategies.push((label, r.map(|r| (r.pitch, r.confidence))));
        }
        let multi = ocr::recognize_pitch_multi(&gray, &data, w, h, &templates);
        let strat_desc: String = strategies
            .iter()
            .map(|(l, r)| match r {
                Some((p, c)) => format!("{l}:{p:.2}({c:.2})"),
                None => format!("{l}:x"),
            })
            .collect::<Vec<_>>()
            .join(" ");
        let match_str = match multi {
            Some(r) => {
                let d = formula::display_distance_for_pitch(r.pitch, formula::DISTANCE_OFFSET_M)
                    .map(|d| format!("{d:.1} m"))
                    .unwrap_or_else(|| "n/a".into());
                let diff = (r.pitch - expected).abs();
                let good = diff < 0.05;
                if good {
                    ok += 1;
                }
                format!(
                    "pitch={:.2} dist={} conf={:.3} mask={} diff={:.2} {}",
                    r.pitch,
                    d,
                    r.confidence,
                    r.strategy_mask,
                    diff,
                    if good { "OK" } else { "MISMATCH" }
                )
            }
            None => format!("NONE (expected {expected})"),
        };
        println!("pic{name}: truth={expected:>7} [{strat_desc}] -> {match_str}");
    }
    println!("accuracy {ok}/{}", truth.len());
}
