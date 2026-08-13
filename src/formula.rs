//! Distance inverse formula for the high-arc throw.
//!
//! Forward formula (fixed contract, never replaced by the low branch):
//!   x = 90 - degrees(asin(y / 60)) / 2
//! where x is the upward elevation in degrees and y the distance in metres.
//! Anchors: 30m -> 75 deg, 60m -> 45 deg.
//!
//! cl_showpos pitch is negative when looking up (Source convention), so the
//! upward elevation is x = -pitch. Inverse:
//!   y = 60 * sin(radians((90 - x) * 2)) = 60 * sin(radians((90 + pitch) * 2))

/// Compute the high-arc distance (metres) for a negative pitch reading.
/// Returns None when the pitch is not a valid looking-up angle:
/// pitch must be in (-90, 0] and the resulting distance within (0, 60].
/// User-measured compensation: the in-game "full damage" distance is 1.5m
/// shorter than the pure formula reads (e.g. hitting a 38m target requires
/// aiming where the tool used to show 39.5m). Displayed distance =
/// formula distance + this offset (default -1.5m, tunable via --offset).
pub const DISTANCE_OFFSET_M: f64 = -1.5;

pub fn distance_for_pitch(pitch_deg: f64) -> Option<f64> {
    if !pitch_deg.is_finite() || pitch_deg > 0.0 || pitch_deg <= -90.0 {
        return None;
    }
    let x = -pitch_deg; // upward elevation
    if x <= 0.0 || x >= 90.0 {
        return None;
    }
    let y = 60.0 * ((90.0 - x) * 2.0).to_radians().sin();
    if !y.is_finite() || y <= 0.0 || y > 60.0 {
        return None;
    }
    Some(y)
}

/// Distance shown to the user: pure formula plus the calibrated offset.
pub fn display_distance_for_pitch(pitch_deg: f64, offset_m: f64) -> Option<f64> {
    let d = distance_for_pitch(pitch_deg)?;
    let shown = d + offset_m;
    if shown.is_finite() && shown >= 0.0 {
        Some(shown)
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn anchors() {
        // -75 deg -> 30m, -45 deg -> 60m (exact contract anchors).
        let d75 = distance_for_pitch(-75.0).unwrap();
        assert!((d75 - 30.0).abs() < 1e-9, "got {d75}");
        let d45 = distance_for_pitch(-45.0).unwrap();
        assert!((d45 - 60.0).abs() < 1e-9, "got {d45}");
    }

    #[test]
    fn monotonic() {
        // Steeper angles (more negative) must give shorter distances.
        let d60 = distance_for_pitch(-60.0).unwrap();
        let d80 = distance_for_pitch(-80.0).unwrap();
        let d88 = distance_for_pitch(-88.0).unwrap();
        assert!(d60 > d80 && d80 > d88);
        assert!((d60 - 51.96).abs() < 0.02); // 60*sin(60deg)
        assert!((d80 - 20.52).abs() < 0.02); // 60*sin(20deg)
    }

    #[test]
    fn display_offset() {
        // 38m real target -> the tool used to show 39.5m: offset -1.5 shows 38.
        // Find the pitch whose formula distance is 39.5 and check display.
        // Formula: y = 60*sin(radians((90+pitch)*2)) = 39.5
        // (90+pitch)*2 = degrees(asin(39.5/60)) -> pitch = asin_deg/2 - 90
        let asin_deg = (39.5f64 / 60.0).asin().to_degrees();
        let pitch = asin_deg / 2.0 - 90.0; // ~ -42.42
        let raw = distance_for_pitch(pitch).unwrap();
        assert!((raw - 39.5).abs() < 0.05, "raw {raw}");
        let shown = display_distance_for_pitch(pitch, -1.5).unwrap();
        assert!((shown - 38.0).abs() < 0.05, "shown {shown}");
        assert_eq!(display_distance_for_pitch(pitch, -1.5).unwrap(), shown);
        // Anchors shift by the same offset.
        let a = display_distance_for_pitch(-75.0, -1.5).unwrap();
        assert!((a - 28.5).abs() < 1e-9, "a {a}");
    }

    #[test]
    fn invalid() {
        assert!(distance_for_pitch(0.0).is_none()); // horizontal
        assert!(distance_for_pitch(75.0).is_none()); // looking down
        assert!(distance_for_pitch(-90.0).is_none()); // vertical limit
        assert!(distance_for_pitch(-95.0).is_none());
        assert!(distance_for_pitch(f64::NAN).is_none());
        assert!(distance_for_pitch(f64::INFINITY).is_none());
    }
}
