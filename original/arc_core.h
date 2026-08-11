#pragma once

#include <cmath>
#include <optional>

namespace arcassist {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDefaultRangeMeters = 60.0;

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

}  // namespace arcassist
