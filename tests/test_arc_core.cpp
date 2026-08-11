// tests/test_arc_core.cpp
// Pure-core tests for src/arc_core.h (portable; macOS clang++ / Linux / Windows).
// Covers TASK_SPEC.md verification list: anchors, invalid values,
// derivative tolerance, 0/360 normalization, both sign modes, numeric
// grammar, glyph helpers, hash reuse, ROI geometry, READY hysteresis,
// stale/invalid reset.

#include "../src/arc_core.h"
#include "../src/roi_geometry.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>

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

using arcassist::BinaryGrid;
using arcassist::ElevationStabilizer;
using arcassist::GlyphTemplate;
using arcassist::ReadyEvent;
using arcassist::ReadyGate;
using arcassist::RoiGeometry;

// Render text using template grids (test helper mirroring the OCR pipeline).
BinaryGrid RenderText(const std::string& text,
                      const std::vector<GlyphTemplate>& tpls, int spacing = 1) {
  const int gw = tpls.front().grid.width;
  const int gh = tpls.front().grid.height;
  int total = 0;
  for (std::size_t i = 0; i < text.size(); ++i) total += gw + spacing;
  total -= spacing;
  BinaryGrid out;
  out.width = total;
  out.height = gh;
  out.pixels.assign(static_cast<std::size_t>(total) * gh, 0);
  int x = 0;
  for (char c : text) {
    for (const auto& t : tpls) {
      if (t.id != c) continue;
      for (int y = 0; y < gh; ++y) {
        for (int xx = 0; xx < gw; ++xx) {
          if (t.grid.At(xx, y)) out.Set(x + xx, y, 1);
        }
      }
    }
    x += gw + spacing;
  }
  return out;
}

// ---------------------------------------------------------------- formula --

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
  CHECK(e30 > 60.0);               // high branch: 75
  CHECK(std::fabs(e30 - 15.0) > 10.0);  // low branch would be 15
  const double e0 = *arcassist::HighArcElevationDegrees(0.0);
  CHECK(e0 > 89.0);                // high branch starts at 90, not 0

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

// ------------------------------------------------------------- tolerance --

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

// ------------------------------------------------------------- pitch map --

void TestPitchNormalization() {
  using arcassist::NormalizePitchDegrees;
  CHECK_NEAR(NormalizePitchDegrees(0.0), 0.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(30.0), 30.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(-30.0), -30.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(350.0), -10.0, 1e-12);   // 0-360 wrap
  CHECK_NEAR(NormalizePitchDegrees(370.0), 10.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(-370.0), -10.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(190.0), -170.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(-190.0), 170.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(180.0), -180.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(-180.0), -180.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(720.0), 0.0, 1e-12);
  CHECK_NEAR(NormalizePitchDegrees(-720.0), 0.0, 1e-12);
}

void TestSignModes() {
  using arcassist::PitchConvention;
  using arcassist::PitchToUpwardElevation;
  const auto neg = PitchConvention::kUpIsNegative;  // Source default
  const auto pos = PitchConvention::kUpIsPositive;

  // Normalized reading first, then map (350 -> -10).
  const double n = arcassist::NormalizePitchDegrees(350.0);
  CHECK_NEAR(*PitchToUpwardElevation(n, neg), 10.0, 1e-12);
  CHECK_NEAR(*PitchToUpwardElevation(n, pos), -10.0, 1e-12);

  // Up is negative: -30 pitch means 30 up; +30 pitch means 30 down.
  CHECK_NEAR(*PitchToUpwardElevation(-30.0, neg), 30.0, 1e-12);
  CHECK_NEAR(*PitchToUpwardElevation(30.0, neg), -30.0, 1e-12);
  CHECK_NEAR(*PitchToUpwardElevation(-90.0, neg), 90.0, 1e-12);
  CHECK_NEAR(*PitchToUpwardElevation(0.0, neg), 0.0, 1e-12);

  // Up is positive: +30 pitch means 30 up.
  CHECK_NEAR(*PitchToUpwardElevation(30.0, pos), 30.0, 1e-12);
  CHECK_NEAR(*PitchToUpwardElevation(-30.0, pos), -30.0, 1e-12);
  CHECK_NEAR(*PitchToUpwardElevation(90.0, pos), 90.0, 1e-12);

  // Physical range and pipeline-order guards.
  CHECK(!PitchToUpwardElevation(95.0, neg).has_value());
  CHECK(!PitchToUpwardElevation(-95.0, pos).has_value());
  CHECK(!PitchToUpwardElevation(350.0, neg).has_value());   // must normalize first
  CHECK(!PitchToUpwardElevation(std::nan(""), neg).has_value());
}

// -------------------------------------------------------------- grammar ---

void TestNumericGrammar() {
  using arcassist::ParsePitchToken;
  CHECK_NEAR(*ParsePitchToken("75"), 75.0, 1e-12);
  CHECK_NEAR(*ParsePitchToken("-12.3"), -12.3, 1e-12);
  CHECK_NEAR(*ParsePitchToken("+7.5"), 7.5, 1e-12);
  CHECK_NEAR(*ParsePitchToken("0.35"), 0.35, 1e-12);
  CHECK_NEAR(*ParsePitchToken("89.522513"), 89.522513, 1e-9);
  CHECK_NEAR(*ParsePitchToken(" 12.5 "), 12.5, 1e-12);  // trims
  CHECK_NEAR(*ParsePitchToken("-0"), 0.0, 1e-12);
  CHECK(!ParsePitchToken("").has_value());
  CHECK(!ParsePitchToken(" ").has_value());
  CHECK(!ParsePitchToken("-").has_value());
  CHECK(!ParsePitchToken("+").has_value());
  CHECK(!ParsePitchToken("12.").has_value());     // dot without trailing digit
  CHECK(!ParsePitchToken(".5").has_value());      // leading dot
  CHECK(!ParsePitchToken("1.2.3").has_value());   // two dots
  CHECK(!ParsePitchToken("abc").has_value());
  CHECK(!ParsePitchToken("12a").has_value());     // trailing junk
  CHECK(!ParsePitchToken("1e2").has_value());     // exponent not in glyph set
  CHECK(!ParsePitchToken("12 3").has_value());
  CHECK(!ParsePitchToken("95").has_value());      // beyond physical pitch range
  CHECK(!ParsePitchToken("-95").has_value());
}

void TestExtractFromText() {
  using arcassist::ExtractPitchFromText;
  CHECK_NEAR(*ExtractPitchFromText("pitch: -12.3"), -12.3, 1e-12);
  CHECK_NEAR(*ExtractPitchFromText("x = 75.0"), 75.0, 1e-12);
  CHECK_NEAR(*ExtractPitchFromText("up: 89.5 down"), 89.5, 1e-12);
  CHECK_NEAR(*ExtractPitchFromText("ang 12."), 12.0, 1e-12);  // maximal token
  CHECK_NEAR(*ExtractPitchFromText("1.2.3"), 1.2, 1e-12);     // first token
  CHECK_NEAR(*ExtractPitchFromText("abc-def 3.5"), 3.5, 1e-12);
  CHECK(!ExtractPitchFromText("").has_value());
  CHECK(!ExtractPitchFromText("no numbers here").has_value());
  CHECK(!ExtractPitchFromText("pitch: 95.0").has_value());    // physical reject
  CHECK(!ExtractPitchFromText("pitch: -95.0").has_value());
}

// -------------------------------------------------------------- glyphs ----

void TestThresholdAndHash() {
  // FNV-1a known vectors.
  CHECK(arcassist::HashBytes("", 0) == 0xcbf29ce484222325ULL);
  CHECK(arcassist::HashBytes("a", 1) == 0xaf63dc4c8601ec8cULL);
  CHECK(arcassist::HashBytes("abc", 3) ==
        arcassist::HashBytes("abc", 3));
  CHECK(arcassist::HashBytes("abc", 3) !=
        arcassist::HashBytes("abd", 3));

  // Threshold bright text.
  const std::uint8_t gray[] = {10, 20, 30, 40, 200, 210, 220, 230};
  const BinaryGrid g = arcassist::ThresholdBright(gray, 4, 2, 128);
  CHECK(g.width == 4 && g.height == 2);
  CHECK(g.pixels == (std::vector<std::uint8_t>{0, 0, 0, 0, 1, 1, 1, 1}));
  const BinaryGrid g2 = arcassist::ThresholdBright(gray, 4, 2, 230);
  CHECK(g2.pixels == (std::vector<std::uint8_t>{0, 0, 0, 0, 0, 0, 0, 1}));

  // Binary ROI hash reuse: identical ROI -> identical hash, any change -> new.
  BinaryGrid roi1;
  roi1.width = 6;
  roi1.height = 3;
  roi1.pixels = {1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1};
  BinaryGrid roi2 = roi1;
  CHECK(arcassist::HashGrid(roi1) == arcassist::HashGrid(roi2));
  BinaryGrid roi3 = roi1;
  roi3.pixels[5] = 1;  // flip one bit
  CHECK(arcassist::HashGrid(roi1) != arcassist::HashGrid(roi3));
  BinaryGrid roi4 = roi1;
  roi4.width = 3;  // dimension change with same byte count
  roi4.height = 6;
  CHECK(arcassist::HashGrid(roi1) != arcassist::HashGrid(roi4));
  // Same bytes, different dims, must differ (dims are hashed in).
  BinaryGrid a5, b5;
  a5.width = 2; a5.height = 2; a5.pixels = {1, 0, 0, 1};
  b5.width = 1; b5.height = 4; b5.pixels = {1, 0, 0, 1};
  CHECK(arcassist::HashGrid(a5) != arcassist::HashGrid(b5));
}

void TestSegmentation() {
  // Two separate blobs -> two components, left-to-right order.
  BinaryGrid roi;
  roi.width = 9;
  roi.height = 3;
  roi.pixels.assign(27, 0);
  roi.Set(1, 1, 1);  // left blob (col 1)
  roi.Set(7, 1, 1);  // right blob (col 7)
  auto comps = arcassist::SegmentGlyphs(roi);
  CHECK(comps.size() == 2);
  CHECK(comps[0].width == 1 && comps[0].height == 1);
  CHECK(comps[0].At(0, 0) == 1);
  CHECK(comps[1].At(0, 0) == 1);

  // 8-connectivity: diagonal touch merges into one component.
  BinaryGrid diag;
  diag.width = 2;
  diag.height = 2;
  diag.pixels = {1, 0, 0, 1};
  CHECK(arcassist::SegmentGlyphs(diag).size() == 1);

  // Empty ROI -> no components.
  BinaryGrid empty;
  empty.width = 4;
  empty.height = 4;
  empty.pixels.assign(16, 0);
  CHECK(arcassist::SegmentGlyphs(empty).empty());
}

void TestNormalizeAndMatch() {
  // '-' (5x1 line) normalizes to the middle row.
  BinaryGrid dash;
  dash.width = 5;
  dash.height = 7;
  dash.pixels.assign(35, 0);
  for (int x = 0; x < 5; ++x) dash.Set(x, 3, 1);
  const BinaryGrid nd = arcassist::NormalizeGlyph(dash, 5, 7);
  CHECK(nd.width == 5 && nd.height == 7);
  CHECK(nd.At(0, 3) == 1 && nd.At(4, 3) == 1);
  CHECK(nd.At(0, 2) == 0 && nd.At(0, 4) == 0);

  // '1'-style 3x7 glyph is centered horizontally.
  BinaryGrid one;
  one.width = 3;
  one.height = 7;
  one.pixels.assign(21, 0);
  for (int y = 0; y < 7; ++y) one.Set(1, y, 1);
  const BinaryGrid n1 = arcassist::NormalizeGlyph(one, 5, 7);
  CHECK(n1.At(2, 0) == 1 && n1.At(1, 0) == 0 && n1.At(3, 0) == 0);
  CHECK(n1.At(2, 6) == 1);

  // 2x2 blob -> aspect-preserving 5x5 block centered.
  BinaryGrid blob;
  blob.width = 2;
  blob.height = 2;
  blob.pixels = {1, 1, 1, 1};
  const BinaryGrid nb = arcassist::NormalizeGlyph(blob, 5, 7);
  CHECK(nb.At(0, 1) == 1 && nb.At(4, 5) == 1);
  CHECK(nb.At(2, 0) == 0 && nb.At(2, 6) == 0);
  // Aspect preserved: 5x5, not stretched to 5x7.
  CHECK(nb.At(0, 0) == 0 && nb.At(4, 6) == 0);

  // Empty glyph -> all-zero grid, no crash.
  BinaryGrid empty;
  empty.width = 3;
  empty.height = 3;
  empty.pixels.assign(9, 0);
  const BinaryGrid ne = arcassist::NormalizeGlyph(empty, 5, 7);
  bool all_zero = true;
  for (std::uint8_t v : ne.pixels) all_zero = all_zero && v == 0;
  CHECK(all_zero);

  // Scores.
  CHECK(arcassist::MatchScore(nd, nd) == 1.0);
  CHECK(arcassist::OverlapScore(nd, nd) == 1.0);
  CHECK(arcassist::MatchScore(nd, nb) < 1.0);
  // Manually disjoint grids score 0.0 (normalized glyphs are centered and
  // therefore always share the middle band).
  BinaryGrid top, bottom;
  top.width = 5; top.height = 7; top.pixels.assign(35, 0);
  bottom.width = 5; bottom.height = 7; bottom.pixels.assign(35, 0);
  for (int x = 0; x < 5; ++x) { top.Set(x, 0, 1); bottom.Set(x, 6, 1); }
  CHECK(arcassist::OverlapScore(top, bottom) == 0.0);
  CHECK(arcassist::OverlapScore(top, top) == 1.0);
  BinaryGrid small;  // dims mismatch -> 0.0, no crash
  small.width = 1;
  small.height = 1;
  small.pixels = {1};
  CHECK(arcassist::MatchScore(nd, small) == 0.0);
  CHECK(arcassist::OverlapScore(nd, small) == 0.0);
}

void TestOcrPipeline() {
  const auto tpls = arcassist::BuildGlyphTemplates();
  CHECK(tpls.size() == 13);  // "-+.0123456789"
  std::string ids;
  for (const auto& t : tpls) ids.push_back(t.id);
  CHECK(ids == "-+.0123456789");
  for (const auto& t : tpls) {
    CHECK(t.grid.width == 5 && t.grid.height == 7);
  }

  // Round trip: render -> segment -> normalize -> classify.
  for (const char* text : {"-12.3", "75.0", "32", "+5.5", "-.5", "0.35", "89.9"}) {
    const BinaryGrid roi = RenderText(text, tpls);
    const std::string rec = arcassist::RecognizeText(roi, tpls);
    CHECK(rec == text);
  }
  // "-.5" is not grammar-valid as a full token but the OCR string itself is
  // preserved; extraction then rejects it (leading dot).
  CHECK(!arcassist::ExtractPitchFromText("-.5").has_value());
  // End-to-end: OCR text -> pitch token.
  const BinaryGrid roi = RenderText("75.0", tpls);
  CHECK_NEAR(*arcassist::ExtractPitchFromText(
                 arcassist::RecognizeText(roi, tpls)),
             75.0, 1e-12);

  // Unrecognized garbage -> '?' and no token.
  BinaryGrid garbage;
  garbage.width = 5;
  garbage.height = 7;
  garbage.pixels.assign(35, 1);  // solid block is not in the glyph set
  const auto c = arcassist::ClassifyGlyph(garbage, tpls, 0.55);
  CHECK(!c.has_value());

  // Best-match sanity: a clean '1' scores 1.0 against '1', less vs '7'.
  auto tpl1 = std::find_if(tpls.begin(), tpls.end(),
                           [](const GlyphTemplate& t) { return t.id == '1'; });
  auto tpl7 = std::find_if(tpls.begin(), tpls.end(),
                           [](const GlyphTemplate& t) { return t.id == '7'; });
  CHECK(tpl1 != tpls.end() && tpl7 != tpls.end());
  CHECK(arcassist::OverlapScore(tpl1->grid, tpl1->grid) == 1.0);
  CHECK(arcassist::OverlapScore(tpl1->grid, tpl7->grid) < 1.0);
  CHECK(*arcassist::ClassifyGlyph(tpl1->grid, tpls, 0.55) == '1');

  // Empty template set -> no classification; every component becomes '?'.
  const std::vector<GlyphTemplate> none;
  CHECK(!arcassist::ClassifyGlyph(tpl1->grid, none).has_value());
  CHECK(arcassist::RecognizeText(roi, none) == "????");
}

// ----------------------------------------------------------- stabilizer ---

void TestStabilizer() {
  ElevationStabilizer st;
  CHECK_NEAR(*st.Push(0.0, 75.0), 75.0, 1e-12);
  CHECK(st.size() == 1);
  CHECK_NEAR(*st.Push(100.0, 75.2), 75.1, 1e-12);  // median of 2
  CHECK_NEAR(*st.Push(200.0, 74.9), 75.0, 1e-12);  // median of 3

  // Implausible jump rejected; history unchanged.
  CHECK(!st.Push(300.0, 95.0).has_value());
  CHECK(st.size() == 3);
  CHECK_NEAR(*st.Push(400.0, 75.1), 75.1, 1e-12);

  // Custom jump limit: 35 deg jump accepted with a 40 deg gate.
  ElevationStabilizer wide(40.0);
  CHECK_NEAR(*wide.Push(0.0, 75.0), 75.0, 1e-12);
  CHECK_NEAR(*wide.Push(100.0, 110.0), 92.5, 1e-12);  // median of 2

  // Stale gap resets the window: far value accepted as fresh data.
  ElevationStabilizer st2;
  CHECK_NEAR(*st2.Push(0.0, 75.0), 75.0, 1e-12);
  CHECK_NEAR(*st2.Push(100.0, 75.2), 75.1, 1e-12);
  CHECK(!st2.Push(200.0, 45.0).has_value());          // jump, fresh -> rejected
  CHECK_NEAR(*st2.Push(2000.0, 45.0), 45.0, 1e-12);   // gap 1800 > 1500 -> reset

  // Reset() clears history.
  st.Reset();
  CHECK(st.size() == 0);
  CHECK_NEAR(*st.Push(0.0, 10.0), 10.0, 1e-12);

  // Non-finite input rejected without disturbing state.
  CHECK(!st.Push(10.0, std::nan("")).has_value());
  CHECK(!st.Push(std::nan(""), 10.0).has_value());
}

// ------------------------------------------------------------- READY gate --

void TestRoiGeometry() {
  RoiGeometry roi{48, 36, 96, 24};
  const RoiGeometry shrunk = arcassist::ResizeRoiCentered(
      roi, -arcassist::kQuickResizeWidthStep,
      -arcassist::kQuickResizeHeightStep);
  CHECK(shrunk.x == 52);
  CHECK(shrunk.y == 38);
  CHECK(shrunk.w == 88);
  CHECK(shrunk.h == 20);

  const RoiGeometry restored = arcassist::ResizeRoiCentered(
      shrunk, arcassist::kQuickResizeWidthStep,
      arcassist::kQuickResizeHeightStep);
  CHECK(restored.x == roi.x);
  CHECK(restored.y == roi.y);
  CHECK(restored.w == roi.w);
  CHECK(restored.h == roi.h);

  const RoiGeometry compact = arcassist::CompactRoiCentered(roi);
  CHECK(compact.x == 60);
  CHECK(compact.y == 39);
  CHECK(compact.w == arcassist::kDefaultRoiWidth);
  CHECK(compact.h == arcassist::kDefaultRoiHeight);

  const RoiGeometry already_compact = arcassist::CompactRoiCentered(compact);
  CHECK(already_compact.x == compact.x);
  CHECK(already_compact.y == compact.y);
  CHECK(already_compact.w == compact.w);
  CHECK(already_compact.h == compact.h);

  const RoiGeometry minimum = arcassist::ResizeRoiCentered(
      {100, 100, 30, 12}, -1000, -1000);
  CHECK(minimum.w == arcassist::kMinRoiWidth);
  CHECK(minimum.h == arcassist::kMinRoiHeight);
  CHECK(minimum.x == 103);
  CHECK(minimum.y == 101);

  const RoiGeometry maximum = arcassist::ResizeRoiCentered(
      {100, 100, 230, 70}, 1000, 1000);
  CHECK(maximum.w == arcassist::kMaxRoiWidth);
  CHECK(maximum.h == arcassist::kMaxRoiHeight);
  CHECK(maximum.x == 95);
  CHECK(maximum.y == 95);

  const RoiGeometry width_only = arcassist::ResizeRoiCentered(roi, -2, 0);
  CHECK(width_only.x == 49);
  CHECK(width_only.y == 36);
  CHECK(width_only.w == 94);
  CHECK(width_only.h == 24);

  const RoiGeometry height_only = arcassist::ResizeRoiCentered(roi, 0, -2);
  CHECK(height_only.x == 48);
  CHECK(height_only.y == 37);
  CHECK(height_only.w == 96);
  CHECK(height_only.h == 22);

  const RoiGeometry odd_shrink = arcassist::ResizeRoiCentered(roi, -1, -1);
  CHECK(odd_shrink.x == 48);
  CHECK(odd_shrink.y == 36);
  CHECK(odd_shrink.w == 95);
  CHECK(odd_shrink.h == 23);
  const RoiGeometry odd_restore =
      arcassist::ResizeRoiCentered(odd_shrink, 1, 1);
  CHECK(odd_restore.x == 48);
  CHECK(odd_restore.y == 36);
  CHECK(odd_restore.w == 96);
  CHECK(odd_restore.h == 24);

  const RoiGeometry left_clamped =
      arcassist::ClampRoiToBounds({-20, 30, 72, 18}, 0, 0, 1920, 1080);
  CHECK(left_clamped.x == 0);
  CHECK(left_clamped.y == 30);
  CHECK(left_clamped.w == 72);
  CHECK(left_clamped.h == 18);

  const RoiGeometry bottom_right_clamped =
      arcassist::ClampRoiToBounds({1900, 1070, 72, 18}, 0, 0, 1920, 1080);
  CHECK(bottom_right_clamped.x == 1848);
  CHECK(bottom_right_clamped.y == 1062);
  CHECK(bottom_right_clamped.w == 72);
  CHECK(bottom_right_clamped.h == 18);

  const RoiGeometry negative_virtual =
      arcassist::ClampRoiToBounds({-2100, -1200, 72, 18}, -1920, -1080,
                                  1920, 1080);
  CHECK(negative_virtual.x == -1920);
  CHECK(negative_virtual.y == -1080);
  CHECK(negative_virtual.w == 72);
  CHECK(negative_virtual.h == 18);

  const RoiGeometry tiny_bounds =
      arcassist::ClampRoiToBounds({0, 0, 72, 18}, 10, 20, 50, 30);
  CHECK(tiny_bounds.x == 10);
  CHECK(tiny_bounds.y == 20);
  CHECK(tiny_bounds.w == 40);
  CHECK(tiny_bounds.h == 10);

  const RoiGeometry invalid_bounds =
      arcassist::ClampRoiToBounds(roi, 10, 10, 10, 10);
  CHECK(invalid_bounds.x == roi.x);
  CHECK(invalid_bounds.y == roi.y);
  CHECK(invalid_bounds.w == roi.w);
  CHECK(invalid_bounds.h == roi.h);
}

void TestReadyEnter() {
  ReadyGate g;
  CHECK(!g.ready());

  // One in-window sample: not enough.
  CHECK(g.Update(0.0, 75.0, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(!g.ready());

  // Second sample too soon (span < 120ms): keep window start.
  CHECK(g.Update(83.0, 75.1, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(!g.ready());

  // Third sample: span from first = 166ms in [120, 500] -> ENTER.
  CHECK(g.Update(166.0, 75.0, 75.0, 0.35) == ReadyEvent::kEntered);
  CHECK(g.ready());

  // Boundary: span exactly 120ms and 500ms enter; 500.1 restarts the window.
  ReadyGate g2;
  g2.Update(0.0, 75.0, 75.0, 0.35);
  CHECK(g2.Update(120.0, 75.0, 75.0, 0.35) == ReadyEvent::kEntered);

  ReadyGate g3;
  g3.Update(0.0, 75.0, 75.0, 0.35);
  CHECK(g3.Update(500.0, 75.0, 75.0, 0.35) == ReadyEvent::kEntered);

  ReadyGate g4;
  g4.Update(0.0, 75.0, 75.0, 0.35);
  CHECK(g4.Update(600.0, 75.0, 75.0, 0.35) == ReadyEvent::kNone);  // 600 > 500: restart
  CHECK(g4.Update(700.0, 75.0, 75.0, 0.35) == ReadyEvent::kNone);  // 100 < 120: wait
  CHECK(g4.Update(900.0, 75.0, 75.0, 0.35) == ReadyEvent::kEntered);  // 300 in window

  // An out-of-window sample breaks the hit streak.
  ReadyGate g5;
  g5.Update(0.0, 75.0, 75.0, 0.35);
  CHECK(g5.Update(200.0, 20.0, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(!g5.ready());
  CHECK(g5.Update(400.0, 75.0, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(g5.Update(600.0, 75.0, 75.0, 0.35) == ReadyEvent::kEntered);

  // Tolerance boundary: error == tolerance is still in-window.
  ReadyGate g6;
  g6.Update(0.0, 74.65, 75.0, 0.35);
  CHECK(g6.Update(200.0, 75.35, 75.0, 0.35) == ReadyEvent::kEntered);
}

void TestReadyExitAndReset() {
  // Two consecutive far samples (error > 2*tolerance) exit.
  ReadyGate g;
  g.Update(0.0, 75.0, 75.0, 0.35);
  CHECK(g.Update(200.0, 75.0, 75.0, 0.35) == ReadyEvent::kEntered);
  CHECK(g.ready());
  CHECK(g.Update(400.0, 74.0, 75.0, 0.35) == ReadyEvent::kNone);  // 1.0 > 0.7
  CHECK(g.ready());  // one far sample is not enough
  CHECK(g.Update(600.0, 72.0, 75.0, 0.35) == ReadyEvent::kExited);
  CHECK(!g.ready());

  // An in-window sample resets the far counter.
  ReadyGate g2;
  g2.Update(0.0, 75.0, 75.0, 0.35);
  g2.Update(200.0, 75.0, 75.0, 0.35);
  CHECK(g2.ready());
  CHECK(g2.Update(400.0, 74.0, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(g2.Update(600.0, 75.0, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(g2.ready());
  CHECK(g2.Update(800.0, 72.0, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(g2.Update(1000.0, 72.0, 75.0, 0.35) == ReadyEvent::kExited);

  // Error exactly 2*tolerance does NOT count as far (strict >).
  ReadyGate g3;
  g3.Update(0.0, 75.0, 75.0, 0.35);
  g3.Update(200.0, 75.0, 75.0, 0.35);
  CHECK(g3.Update(400.0, 75.7, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(g3.ready());

  // Invalid sample (OCR failure) exits immediately.
  ReadyGate g4;
  g4.Update(0.0, 75.0, 75.0, 0.35);
  g4.Update(200.0, 75.0, 75.0, 0.35);
  CHECK(g4.ready());
  CHECK(g4.OnInvalidSample(300.0) == ReadyEvent::kExited);
  CHECK(!g4.ready());
  CHECK(g4.OnInvalidSample(400.0) == ReadyEvent::kNone);  // already reset

  // Non-finite / bad-tolerance Update behaves like an invalid sample.
  ReadyGate g5;
  g5.Update(0.0, 75.0, 75.0, 0.35);
  g5.Update(200.0, 75.0, 75.0, 0.35);
  CHECK(g5.Update(300.0, std::nan(""), 75.0, 0.35) == ReadyEvent::kExited);
  ReadyGate g6;
  g6.Update(0.0, 75.0, 75.0, 0.35);
  g6.Update(200.0, 75.0, 75.0, 0.35);
  CHECK(g6.Update(300.0, 75.0, 75.0, 0.0) == ReadyEvent::kExited);

  // Stale gap (> 1500ms) resets the gate; a later sample restarts fresh.
  ReadyGate g7;
  g7.Update(0.0, 75.0, 75.0, 0.35);
  g7.Update(200.0, 75.0, 75.0, 0.35);
  CHECK(g7.ready());
  CHECK(g7.Update(2000.0, 75.0, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(!g7.ready());
  CHECK(g7.Update(2100.0, 75.0, 75.0, 0.35) == ReadyEvent::kNone);  // 100 < 120: wait
  CHECK(g7.Update(2300.0, 75.0, 75.0, 0.35) == ReadyEvent::kEntered);  // 300 in window

  // Reset() clears everything.
  ReadyGate g8;
  g8.Update(0.0, 75.0, 75.0, 0.35);
  g8.Update(200.0, 75.0, 75.0, 0.35);
  CHECK(g8.ready());
  g8.Reset();
  CHECK(!g8.ready());
  CHECK(g8.Update(0.0, 75.0, 75.0, 0.35) == ReadyEvent::kNone);
  CHECK(!g8.ready());

  // Distance change (new target) -> Reset -> hysteresis must re-arm.
  ReadyGate g9;
  g9.Update(0.0, 75.0, 75.0, 0.35);
  g9.Update(200.0, 75.0, 75.0, 0.35);
  CHECK(g9.ready());
  g9.Reset();  // user re-entered distance 40m -> target 69.0948, tol 0.35
  CHECK(g9.Update(0.0, 69.1, 69.0948425521, 0.35) == ReadyEvent::kNone);
  CHECK(g9.Update(200.0, 69.1, 69.0948425521, 0.35) == ReadyEvent::kEntered);
}

}  // namespace

int main() {
  TestFormulaAnchors();
  TestFormulaInvalid();
  TestTolerance();
  TestPitchNormalization();
  TestSignModes();
  TestNumericGrammar();
  TestExtractFromText();
  TestThresholdAndHash();
  TestSegmentation();
  TestNormalizeAndMatch();
  TestOcrPipeline();
  TestStabilizer();
  TestRoiGeometry();
  TestReadyEnter();
  TestReadyExitAndReset();

  std::printf("arc_core: %d checks, %d failures\n", g_checks, g_failures);
  return g_failures == 0 ? 0 : 1;
}
