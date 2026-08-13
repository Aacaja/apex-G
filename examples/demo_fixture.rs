//! End-to-end demo: run the full pipeline on the offline pic.png fixture
//! (the exact ANG row region of the user's real screenshot), printing the
//! recognized pitch and the displayed distance. Cross-platform; no capture.

use apex_angle_reader::{formula, ocr, templates};

mod fixtures {
    include!(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/ang_roi.rs"
    ));
}

fn main() {
    let bin = ocr::BinaryImage {
        width: fixtures::WIDTH,
        height: fixtures::HEIGHT,
        pixels: fixtures::PIXELS.to_vec(),
    };
    let templates = templates::build_normalized();
    let glyphs = ocr::segment(&bin);
    println!(
        "pic.png ANG-region fixture: {}x{} foreground={} components={}",
        fixtures::WIDTH,
        fixtures::HEIGHT,
        fixtures::FOREGROUND,
        glyphs.len()
    );
    match ocr::recognize_pitch(&bin, &templates) {
        Some(r) => match formula::distance_for_pitch(r.pitch) {
            Some(d) => println!(
                "RESULT pitch={:.2} deg confidence={:.3} -> distance={:.1} m",
                r.pitch, r.confidence, d
            ),
            None => println!("RESULT pitch={:.2} but out of formula range", r.pitch),
        },
        None => println!("RESULT none (fail-closed, overlay would show --)"),
    }
    // Ground truth from the screenshot: ANG: -52.25 -90.23 0.00
    println!("GROUND_TRUTH pitch=-52.25");
}
