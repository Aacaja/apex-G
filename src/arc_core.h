#pragma once
// Arc Assist portable core — angle-calculator edition.
//
// This file is a copy of original/arc_core.h (which is preserved unchanged),
// extended ONLY with the shared constants used by the UI and tests:
//   - high-arc formula (kept byte-identical to the original)
//   - derivative-based sensitivity tolerance
//
// The user formula is the HIGH branch:  x = 90 - degrees(asin(y/60)) / 2
// (30m -> 75 degrees, 60m -> 45 degrees).  It must never be replaced by the
// low branch degrees(asin(y/60))/2.
//
// Header-only C++17, no platform dependencies.

#include <algorithm>
#include <cmath>
#include <optional>

namespace arcassist {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDefaultRangeMeters = 60.0;
constexpr double kMinDistanceMeters = 1.0;   // UI distance entry range
constexpr double kMaxDistanceMeters = 60.0;

// --- formula (unchanged from original/arc_core.h) -------------------------

inline std::optional<double> HighArcElevationDegrees(
    double distance_m,
    double range_m = kDefaultRangeMeters) {
  if (!std::isfinite(distance_m) || !std::isfinite(range_m) ||
      range_m <= 0.0 || distance_m < 0.0 || distance_m > range_m) {
    return std::nullopt;
  }
  const double ratio = distance_m / range_m;
  const double asin_degrees = std::asin(ratio) * 180.0 / kPi;
  return 90.0 - asin_degrees / 2.0;
}

// --- derivative-based sensitivity tolerance --------------------------------
// e(d) = 90 - (90/pi)*asin(d/60)
// de/dd = -(1.5/pi) / sqrt(1 - (d/60)^2)   (-> -inf at d = 60)
// Tolerance = clamp(0.5 * |de/dd|, 0.35, 2.0) degrees so that integer ping
// rounding stays usable near 60m.
constexpr double kToleranceFloorDeg = 0.35;
constexpr double kToleranceCeilDeg = 2.0;

inline double ElevationToleranceDegrees(double distance_m) {
  if (!std::isfinite(distance_m) || distance_m <= 0.0 ||
      distance_m >= kDefaultRangeMeters) {
    // Non-finite / out-of-domain input: fall back to the narrow (safe) floor
    // except at the 60m endpoint where the derivative is infinite -> ceiling.
    return (std::isfinite(distance_m) && distance_m >= kDefaultRangeMeters)
               ? kToleranceCeilDeg
               : kToleranceFloorDeg;
  }
  const double r = distance_m / kDefaultRangeMeters;
  const double half_deriv = 0.5 * 1.5 / (kPi * std::sqrt(1.0 - r * r));
  return std::clamp(half_deriv, kToleranceFloorDeg, kToleranceCeilDeg);
}

}  // namespace arcassist
