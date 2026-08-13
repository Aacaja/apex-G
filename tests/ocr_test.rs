//! Cross-platform OCR pipeline tests against the offline pic.png fixture.

use apex_angle_reader::ocr::{self, BinaryImage};
use apex_angle_reader::templates;

mod fixtures {
    include!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/ang_roi.rs"
    ));
}

fn fixture_binary() -> BinaryImage {
    BinaryImage {
        width: fixtures::WIDTH,
        height: fixtures::HEIGHT,
        pixels: fixtures::PIXELS.to_vec(),
    }
}

#[test]
fn fixture_integrity() {
    assert_eq!(fixtures::WIDTH, 240);
    assert_eq!(fixtures::HEIGHT, 55);
    assert_eq!(
        fixtures::PIXELS.iter().filter(|v| **v == 1).count(),
        fixtures::FOREGROUND,
        "foreground count must match the recorded value"
    );
    assert_eq!(fixtures::FOREGROUND, 1409);
}

#[test]
fn recognizes_known_pitch_from_screenshot() {
    let bin = fixture_binary();
    let templates = templates::build_normalized();
    let result = ocr::recognize_pitch(&bin, &templates)
        .expect("ANG row with negative pitch must be recognized");
    // pic.png shows ANG: -52.25 ...
    assert!(
        (result.pitch - (-52.25)).abs() < 0.01,
        "pitch = {}",
        result.pitch
    );
    assert!(
        result.confidence > 0.7,
        "confidence = {}",
        result.confidence
    );
}

#[test]
fn rejects_missing_minus() {
    // Erase the leading minus of -52.25: fixture-local (52..55, 30).
    let mut bin = fixture_binary();
    for x in 52..56 {
        bin.pixels[30 * fixtures::WIDTH + x] = 0;
    }
    let templates = templates::build_normalized();
    // Without the sign the pitch must NOT be reported (fail-closed).
    assert!(ocr::recognize_pitch(&bin, &templates).is_none());
}

#[test]
fn rejects_blank_frame() {
    let templates = templates::build_normalized();
    let blank = BinaryImage {
        width: 240,
        height: 55,
        pixels: vec![0; 240 * 55],
    };
    assert!(ocr::recognize_pitch(&blank, &templates).is_none());
}

#[test]
fn formula_matches_ocr() {
    // The pipeline maps -52.25 to the inverse-formula distance.
    let d = apex_angle_reader::formula::distance_for_pitch(-52.25).unwrap();
    // x = 52.25 deg elevation -> y = 60*sin((90-52.25)*2 deg) = 60*sin(75.5 deg)
    let expected = 60.0 * (75.5f64).to_radians().sin();
    assert!((d - expected).abs() < 1e-9);
}
