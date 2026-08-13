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
    fn invalid() {
        assert!(distance_for_pitch(0.0).is_none()); // horizontal
        assert!(distance_for_pitch(75.0).is_none()); // looking down
        assert!(distance_for_pitch(-90.0).is_none()); // vertical limit
        assert!(distance_for_pitch(-95.0).is_none());
        assert!(distance_for_pitch(f64::NAN).is_none());
        assert!(distance_for_pitch(f64::INFINITY).is_none());
    }
}
