// tests/test_arc_core.cpp
// Pure-core tests for src/arc_core.h (portable; macOS clang++ / Linux / Windows).
// Covers the angle-calculator contract: high-arc formula anchors, invalid
// values, monotonicity, derivative tolerance, and the 10-60m ruler table.

#include "../src/arc_core.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                          \
  do {                                                                       \
    ++g_checks;                                                              \
    if (!(cond)) {                                                           \
      ++g_failures;                                                          \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
    }                                                                        \
  } while (0)

#define CHECK_NEAR(a, b, eps)                                                \
  do {                                                                       \
    ++g_checks;                                                              \
    const double _a = (a), _b = (b);                                         \
    if (!(std::fabs(_a - _b) <= (eps))) {                                    \
      ++g_failures;                                                          \
      std::printf("FAIL %s:%d: |%s - %s| = |%.10f - %.10f| > %s\n",          \
                  __FILE__, __LINE__, #a, #b, _a, _b, #eps);                 \
    }                                                                        \
  } while (0)

void TestFormulaAnchors() {
  // High branch: x = 90 - degrees(asin(y/60))/2. 30m MUST be 75 deg; the low
  // branch degrees(asin(y/60))/2 would give 15 deg at 30m — never that.
  CHECK_NEAR(*arcassist::HighArcElevationDegrees(0.0), 90.0, 1e-9);
  CHECK_NEAR(*arcassist::HighArcElevationDegrees(1.0), 89.5225130631, 1e-6);
  CHECK_NEAR(*arcassist::HighArcElevationDegrees(30.0), 75.0, 1e-9);
  CHECK_NEAR(*arcassist::HighArcElevationDegrees(40.0), 69.0948425521, 1e-6);
  CHECK_NEAR(*arcassist::HighArcElevationDegrees(55.0), 56.7782321546, 1e-6);
  CHECK_NEAR(*arcassist::HighArcElevationDegrees(60.0), 45.0, 1e-9);

  // Explicit anti-low-branch guard (the user formula must stay the high arc).
  const double e30 = *arcassist::HighArcElevationDegrees(30.0);
  CHECK(e30 > 60.0);                    // high branch: 75
  CHECK(std::fabs(e30 - 15.0) > 10.0);  // low branch would be 15
  const double e0 = *arcassist::HighArcElevationDegrees(0.0);
  CHECK(e0 > 89.0);                     // high branch starts at 90, not 0

  // Monotonic decreasing with distance.
  const double d40 = *arcassist::HighArcElevationDegrees(40.0);
  const double d55 = *arcassist::HighArcElevationDegrees(55.0);
  CHECK(e30 > d40 && d40 > d55 && d55 > 45.0);

  // Custom range parameter.
  CHECK_NEAR(*arcassist::HighArcElevationDegrees(15.0, 30.0), 75.0, 1e-9);
  CHECK_NEAR(*arcassist::HighArcElevationDegrees(30.0, 30.0), 45.0, 1e-9);
}

void TestFormulaInvalid() {
  using arcassist::HighArcElevationDegrees;
  CHECK(!HighArcElevationDegrees(-1.0).has_value());        // negative
  CHECK(!HighArcElevationDegrees(60.5).has_value());        // beyond range
  CHECK(!HighArcElevationDegrees(100.0).has_value());
  CHECK(!HighArcElevationDegrees(std::nan("")).has_value());
  CHECK(!HighArcElevationDegrees(std::numeric_limits<double>::infinity()).has_value());
  CHECK(!HighArcElevationDegrees(-std::numeric_limits<double>::infinity()).has_value());
  CHECK(!HighArcElevationDegrees(10.0, 0.0).has_value());   // bad range
  CHECK(!HighArcElevationDegrees(10.0, -5.0).has_value());
  CHECK(!HighArcElevationDegrees(10.0, std::nan("")).has_value());
  CHECK(HighArcElevationDegrees(60.0).has_value());         // endpoint valid
  CHECK(HighArcElevationDegrees(0.0).has_value());          // 0 valid -> 90
}

void TestTolerance() {
  using arcassist::ElevationToleranceDegrees;
  const double kFloor = arcassist::kToleranceFloorDeg;
  const double kCeil = arcassist::kToleranceCeilDeg;

  // Floored near 0 (derivative 0.4775 -> half 0.2387 < 0.35).
  CHECK_NEAR(ElevationToleranceDegrees(0.0), kFloor, 1e-12);
  CHECK_NEAR(ElevationToleranceDegrees(1.0), kFloor, 1e-12);
  CHECK_NEAR(ElevationToleranceDegrees(30.0), kFloor, 1e-12);  // 0.2757 -> floor

  // Analytic value at 55m: 0.5*1.5/(pi*sqrt(1-(55/60)^2)).
  CHECK_NEAR(ElevationToleranceDegrees(55.0), 0.5973497946556853, 1e-9);
  // Unclamped region (>= ~43.9m): matches 0.5*|de/dd| from numeric derivative.
  const double h = 1e-4;
  const double numeric_deriv =
      (*arcassist::HighArcElevationDegrees(55.0 + h) -
       *arcassist::HighArcElevationDegrees(55.0 - h)) / (2.0 * h);
  CHECK_NEAR(ElevationToleranceDegrees(55.0), 0.5 * std::fabs(numeric_deriv),
             1e-3);

  // Capped at the 60m endpoint (infinite derivative).
  CHECK_NEAR(ElevationToleranceDegrees(60.0), kCeil, 1e-12);
  CHECK_NEAR(ElevationToleranceDegrees(59.9), kCeil, 1e-12);  // half 4.14 -> cap
  CHECK(ElevationToleranceDegrees(59.0) < kCeil);

  // Monotonic non-decreasing; always within the clamp.
  double prev = -1.0;
  for (double d = 0.0; d <= 60.0; d += 0.5) {
    const double t = ElevationToleranceDegrees(d);
    CHECK(t >= kFloor - 1e-12 && t <= kCeil + 1e-12);
    CHECK(t >= prev - 1e-12);
    prev = t;
  }

  // Invalid inputs fall back to the safe narrow floor.
  CHECK_NEAR(ElevationToleranceDegrees(-5.0), kFloor, 1e-12);
  CHECK_NEAR(ElevationToleranceDegrees(std::nan("")), kFloor, 1e-12);
}

void TestRulerTable() {
  // The UI ruler shows 10m..60m at 10m steps. Expected displayed values
  // (one decimal): 85.2 / 80.3 / 75.0 / 69.1 / 61.8 / 45.0.
  const double expected[6] = {85.2, 80.3, 75.0, 69.1, 61.8, 45.0};
  for (int i = 0; i < 6; ++i) {
    const double distance = 10.0 * (i + 1);
    const auto angle = arcassist::HighArcElevationDegrees(distance);
    CHECK(angle.has_value());
    if (angle) {
      const double rounded = std::round(*angle * 10.0) / 10.0;
      CHECK_NEAR(rounded, expected[i], 0.05);
    }
  }
}

}  // namespace

int main() {
  TestFormulaAnchors();
  TestFormulaInvalid();
  TestTolerance();
  TestRulerTable();
  std::printf("arc_core: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
