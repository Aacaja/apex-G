#pragma once
// Arc Assist portable core.
//
// This file is a copy of original/arc_core.h (which is preserved unchanged),
// extended ONLY with portable, platform-free logic per TASK_SPEC.md:
//   - high-arc formula (kept byte-identical to the original)
//   - derivative-based sensitivity tolerance (clamp(0.5*|de/dd|, 0.35, 2.0))
//   - pitch 0-360 normalization and sign-convention mapping
//   - strict numeric grammar for pitch tokens
//   - binary-glyph segmentation / normalization / template matching
//   - FNV-1a binary ROI hashing for parse reuse
//   - elevation stabilizer (jump / stale rejection)
//   - READY gate with enter/exit hysteresis
//
// The user formula is the HIGH branch:  x = 90 - degrees(asin(y/60)) / 2
// (30m -> 75 degrees, 60m -> 45 degrees).  It must never be replaced by the
// low branch degrees(asin(y/60))/2.
//
// Header-only C++17, no platform dependencies (no Win32, no GPU, no OCR SDK).

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arcassist {

constexpr double kPi = 3.141592653589793238462643383279502884;
constexpr double kDefaultRangeMeters = 60.0;

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

// --- shared constants ------------------------------------------------------

constexpr double kMinDistanceMeters = 1.0;   // UI distance entry range
constexpr double kMaxDistanceMeters = 60.0;
constexpr double kMaxPitchDeg = 90.0;        // physical cl_showpos pitch range

constexpr double kToleranceFloorDeg = 0.35;  // READY tolerance clamp [deg]
constexpr double kToleranceCeilDeg = 2.0;
constexpr double kReadyMinSpanMs = 120.0;    // READY enter window [ms]
constexpr double kReadyMaxSpanMs = 500.0;
constexpr double kReadyExitFactor = 2.0;     // exit after 2 samples beyond 2*tol
constexpr double kStaleSampleMs = 1500.0;    // gate/stabilizer stale reset
constexpr double kMaxJumpDeg = 15.0;         // stabilizer jump rejection
constexpr std::size_t kStabilizerWindow = 3;

// --- derivative-based sensitivity tolerance --------------------------------
// e(d) = 90 - (90/pi)*asin(d/60)
// de/dd = -(1.5/pi) / sqrt(1 - (d/60)^2)   (-> -inf at d = 60)
// Tolerance = clamp(0.5 * |de/dd|, 0.35, 2.0) degrees so that integer ping
// rounding stays usable near 60m.

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

// --- pitch normalization and sign mapping ----------------------------------
// Source (cl_showpos) convention: looking up reads NEGATIVE pitch; the user
// can toggle to "up is positive".

enum class PitchConvention { kUpIsNegative = 0, kUpIsPositive = 1 };

// Wrap any reading (0-360 or signed) into the signed range [-180, 180).
inline double NormalizePitchDegrees(double raw_deg) {
  double r = std::fmod(raw_deg, 360.0);
  if (r >= 180.0) r -= 360.0;
  else if (r < -180.0) r += 360.0;
  return r;
}

// Map a (normalized/signed) pitch reading to upward elevation in degrees.
// Rejects non-finite, unnormalized (|pitch| > 180) and physically impossible
// (|pitch| > 90) readings.
inline std::optional<double> PitchToUpwardElevation(
    double pitch_deg, PitchConvention conv) {
  if (!std::isfinite(pitch_deg) || std::fabs(pitch_deg) > 180.0 ||
      std::fabs(pitch_deg) > kMaxPitchDeg) {
    return std::nullopt;
  }
  return (conv == PitchConvention::kUpIsNegative) ? -pitch_deg : pitch_deg;
}

// Strict numeric grammar: [+-]? dd.dd  (sign optional, >=1 digits, optional
// '.' followed by >=1 digits, nothing else; whole trimmed string must match).
// Rejects values outside the physical pitch range (+-90).
inline std::optional<double> ParsePitchToken(std::string_view text) {
  std::size_t b = 0, e = text.size();
  while (b < e && std::isspace(static_cast<unsigned char>(text[b]))) ++b;
  while (e > b && std::isspace(static_cast<unsigned char>(text[e - 1]))) --e;
  const std::string_view t = text.substr(b, e - b);
  if (t.empty()) return std::nullopt;

  std::size_t i = 0;
  if (t[i] == '+' || t[i] == '-') ++i;
  if (i >= t.size() || !std::isdigit(static_cast<unsigned char>(t[i]))) {
    return std::nullopt;
  }
  while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) ++i;
  if (i < t.size() && t[i] == '.') {
    ++i;
    if (i >= t.size() || !std::isdigit(static_cast<unsigned char>(t[i]))) {
      return std::nullopt;
    }
    while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) ++i;
  }
  if (i != t.size()) return std::nullopt;

  double sign = 1.0;
  double v = 0.0;
  double div = 1.0;
  bool frac = false;
  for (std::size_t k = (t[0] == '+' || t[0] == '-') ? 1 : 0; k < t.size();
       ++k) {
    if (t[k] == '.') { frac = true; continue; }
    if (frac) { div *= 10.0; v += static_cast<double>(t[k] - '0') / div; }
    else v = v * 10.0 + static_cast<double>(t[k] - '0');
  }
  if (t[0] == '-') sign = -1.0;
  v *= sign;

  if (!std::isfinite(v) || std::fabs(v) > kMaxPitchDeg) return std::nullopt;
  return v;
}

// Find the first maximal grammar-valid numeric token in arbitrary OCR text
// (e.g. "pitch: -12.3" -> -12.3, "12." -> 12). A token outside the physical
// pitch range rejects the whole reading (return nullopt), as does a candidate
// directly following an unconsumed '.' (mangled fraction, e.g. "-.5").
inline std::optional<double> ExtractPitchFromText(std::string_view text) {
  std::size_t i = 0;
  while (i < text.size()) {
    if (std::isspace(static_cast<unsigned char>(text[i]))) { ++i; continue; }
    // A token directly after '.' would be the mangled fraction of a previous
    // token (e.g. "-.5"); reject such candidates instead of misreading "5".
    if (i > 0 && text[i - 1] == '.') { ++i; continue; }
    std::size_t j = i;
    if (text[j] == '+' || text[j] == '-') ++j;
    const std::size_t dstart = j;
    while (j < text.size() &&
           std::isdigit(static_cast<unsigned char>(text[j]))) ++j;
    if (j == dstart) { ++i; continue; }
    if (j < text.size() && text[j] == '.') {
      const std::size_t k = j + 1;
      if (k < text.size() && std::isdigit(static_cast<unsigned char>(text[k]))) {
        std::size_t m = k;
        while (m < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[m]))) ++m;
        j = m;
      }
    }
    const auto v = ParsePitchToken(text.substr(i, j - i));
    if (v) return v;
    return std::nullopt;  // grammar matched but out of physical range
  }
  return std::nullopt;
}

// --- binary grids ----------------------------------------------------------

struct BinaryGrid {
  int width = 0;
  int height = 0;
  std::vector<std::uint8_t> pixels;  // 1 = foreground (bright HUD text)

  std::uint8_t At(int x, int y) const {
    if (x < 0 || y < 0 || x >= width || y >= height) return 0;
    return pixels[static_cast<std::size_t>(y) * width + x];
  }
  void Set(int x, int y, std::uint8_t v) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    pixels[static_cast<std::size_t>(y) * width + x] = v ? 1 : 0;
  }
};

inline BinaryGrid ThresholdBright(const std::uint8_t* gray, int w, int h,
                                  std::uint8_t threshold) {
  BinaryGrid g;
  g.width = w;
  g.height = h;
  if (w <= 0 || h <= 0 || gray == nullptr) return g;
  g.pixels.resize(static_cast<std::size_t>(w) * h);
  for (std::size_t i = 0; i < g.pixels.size(); ++i) {
    g.pixels[i] = gray[i] >= threshold ? 1 : 0;
  }
  return g;
}

// 8-connected component segmentation; components sorted left-to-right (then
// top-to-bottom). Each component is cropped to its bounding box.
inline std::vector<BinaryGrid> SegmentGlyphs(const BinaryGrid& roi) {
  std::vector<BinaryGrid> out;
  if (roi.width <= 0 || roi.height <= 0) return out;

  struct Component { BinaryGrid grid; int x0; int y0; };
  std::vector<Component> comps;
  std::vector<std::uint8_t> visited(
      static_cast<std::size_t>(roi.width) * roi.height, 0);
  std::vector<std::pair<int, int>> stack;

  for (int y = 0; y < roi.height; ++y) {
    for (int x = 0; x < roi.width; ++x) {
      if (!roi.At(x, y) || visited[static_cast<std::size_t>(y) * roi.width + x]) {
        continue;
      }
      int x0 = x, y0 = y, x1 = x, y1 = y;
      std::vector<std::pair<int, int>> cells;
      stack.clear();
      stack.push_back({x, y});
      visited[static_cast<std::size_t>(y) * roi.width + x] = 1;
      while (!stack.empty()) {
        const auto [cx, cy] = stack.back();
        stack.pop_back();
        cells.push_back({cx, cy});
        x0 = std::min(x0, cx); y0 = std::min(y0, cy);
        x1 = std::max(x1, cx); y1 = std::max(y1, cy);
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int nx = cx + dx, ny = cy + dy;
            if (nx < 0 || ny < 0 || nx >= roi.width || ny >= roi.height) continue;
            const std::size_t idx = static_cast<std::size_t>(ny) * roi.width + nx;
            if (!roi.At(nx, ny) || visited[idx]) continue;
            visited[idx] = 1;
            stack.push_back({nx, ny});
          }
        }
      }
      BinaryGrid g;
      g.width = x1 - x0 + 1;
      g.height = y1 - y0 + 1;
      g.pixels.assign(static_cast<std::size_t>(g.width) * g.height, 0);
      for (const auto& [cx, cy] : cells) g.Set(cx - x0, cy - y0, 1);
      comps.push_back({std::move(g), x0, y0});
    }
  }
  std::sort(comps.begin(), comps.end(),
            [](const Component& a, const Component& b) {
              if (a.x0 != b.x0) return a.x0 < b.x0;
              return a.y0 < b.y0;
            });
  for (auto& c : comps) out.push_back(std::move(c.grid));
  return out;
}

// Normalize a glyph to target_w x target_h: aspect-preserving fit of the
// foreground bounding box, centered. Empty input yields an all-zero grid.
inline BinaryGrid NormalizeGlyph(const BinaryGrid& g, int target_w,
                                 int target_h) {
  BinaryGrid out;
  out.width = target_w;
  out.height = target_h;
  if (g.width <= 0 || g.height <= 0 || target_w <= 0 || target_h <= 0) return out;
  out.pixels.assign(static_cast<std::size_t>(target_w) * target_h, 0);

  int x0 = g.width, y0 = g.height, x1 = -1, y1 = -1;
  for (int y = 0; y < g.height; ++y) {
    for (int x = 0; x < g.width; ++x) {
      if (!g.At(x, y)) continue;
      x0 = std::min(x0, x); y0 = std::min(y0, y);
      x1 = std::max(x1, x); y1 = std::max(y1, y);
    }
  }
  if (x1 < x0) return out;  // empty glyph

  const int bw = x1 - x0 + 1, bh = y1 - y0 + 1;
  const double scale = std::min(static_cast<double>(target_w) / bw,
                                static_cast<double>(target_h) / bh);
  int sw = std::max(1, static_cast<int>(std::lround(bw * scale)));
  int sh = std::max(1, static_cast<int>(std::lround(bh * scale)));
  sw = std::min(sw, target_w);
  sh = std::min(sh, target_h);
  const int ox = (target_w - sw) / 2;
  const int oy = (target_h - sh) / 2;
  for (int y = 0; y < sh; ++y) {
    for (int x = 0; x < sw; ++x) {
      const int sx = x0 + static_cast<int>(x / scale);
      const int sy = y0 + static_cast<int>(y / scale);
      if (sx <= x1 && sy <= y1) out.Set(ox + x, oy + y, g.At(sx, sy));
    }
  }
  return out;
}

// Fraction of equal pixels; requires identical dimensions.
inline double MatchScore(const BinaryGrid& a, const BinaryGrid& b) {
  if (a.width != b.width || a.height != b.height || a.pixels.empty()) return 0.0;
  std::size_t same = 0;
  for (std::size_t i = 0; i < a.pixels.size(); ++i) {
    if (a.pixels[i] == b.pixels[i]) ++same;
  }
  return static_cast<double>(same) / a.pixels.size();
}

// Intersection over max-cardinality (foreground-aware); requires identical
// dimensions. Robust against background agreement between sparse glyphs.
inline double OverlapScore(const BinaryGrid& a, const BinaryGrid& b) {
  if (a.width != b.width || a.height != b.height) return 0.0;
  std::size_t inter = 0, ca = 0, cb = 0;
  for (std::size_t i = 0; i < a.pixels.size(); ++i) {
    ca += a.pixels[i];
    cb += b.pixels[i];
    if (a.pixels[i] && b.pixels[i]) ++inter;
  }
  const std::size_t m = std::max(ca, cb);
  return m == 0 ? 0.0 : static_cast<double>(inter) / m;
}

// --- FNV-1a 64-bit hash of the binary ROI (parse-reuse) --------------------

inline std::uint64_t HashBytesContinue(std::uint64_t h, const void* data,
                                       std::size_t n) {
  const auto* p = static_cast<const std::uint8_t*>(data);
  for (std::size_t i = 0; i < n; ++i) {
    h ^= p[i];
    h *= 1099511628211ULL;  // FNV prime
  }
  return h;
}

inline std::uint64_t HashBytes(const void* data, std::size_t n) {
  return HashBytesContinue(14695981039346656037ULL, data, n);
}

inline std::uint64_t HashGrid(const BinaryGrid& g) {
  std::uint64_t h =
      HashBytesContinue(14695981039346656037ULL, &g.width, sizeof(g.width));
  h = HashBytesContinue(h, &g.height, sizeof(g.height));
  h = HashBytesContinue(h, g.pixels.data(), g.pixels.size());
  return h;
}

// --- generated common-font templates + matching ----------------------------

struct GlyphTemplate {
  char id;
  BinaryGrid grid;
};

// 5x7 bitmap fallback for the fixed "-+.0123456789" glyph set. Templates are
// pushed through NormalizeGlyph so they are centered exactly like unknown
// glyphs (e.g. the '.' dot becomes a centered 5x3 block).
inline std::vector<GlyphTemplate> BuildGlyphTemplates() {
  static const char* kFont[13][7] = {
    // '-' '+' '.' '0' '1' '2' '3' '4' '5' '6' '7' '8' '9'
    { ".....", ".....", ".....", "#####", ".....", ".....", "....." },  // -
    { ".....", "..#..", "..#..", "#####", "..#..", "..#..", "....." },  // +
    { ".....", ".....", ".....", ".....", ".###.", ".###.", "....." },  // .
    { ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###." },  // 0
    { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." },  // 1
    { ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" },  // 2
    { ".###.", "#...#", "....#", "..##.", "....#", "#...#", ".###." },  // 3
    { "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#." },  // 4
    { "#####", "#....", "####.", "....#", "....#", "#...#", ".###." },  // 5
    { "..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###." },  // 6
    { "#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..." },  // 7
    { ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###." },  // 8
    { ".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.." },  // 9
  };
  static const char kIds[13] = {'-', '+', '.', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9'};
  std::vector<GlyphTemplate> out;
  out.reserve(13);
  for (int i = 0; i < 13; ++i) {
    BinaryGrid g;
    g.width = 5;
    g.height = 7;
    g.pixels.assign(35, 0);
    for (int y = 0; y < 7; ++y) {
      for (int x = 0; x < 5; ++x) {
        if (kFont[i][y][x] == '#') g.Set(x, y, 1);
      }
    }
    out.push_back({kIds[i], NormalizeGlyph(g, 5, 7)});
  }
  return out;
}

// Classify one (any-size) binary glyph against the template set. The glyph is
// normalized to the template grid, then scored with OverlapScore.
inline std::optional<char> ClassifyGlyph(
    const BinaryGrid& glyph, const std::vector<GlyphTemplate>& templates,
    double min_score = 0.55) {
  if (templates.empty()) return std::nullopt;
  const int tw = templates.front().grid.width;
  const int th = templates.front().grid.height;
  const BinaryGrid ng = NormalizeGlyph(glyph, tw, th);
  char best = '\0';
  double best_score = 0.0;
  for (const auto& t : templates) {
    const double s = OverlapScore(ng, t.grid);
    if (s > best_score) { best_score = s; best = t.id; }
  }
  return best_score >= min_score ? std::optional<char>(best) : std::nullopt;
}

// Full glyph OCR over a binary ROI: segment -> normalize -> classify.
// Unrecognized components become '?'.
inline std::string RecognizeText(const BinaryGrid& roi,
                                 const std::vector<GlyphTemplate>& templates,
                                 double min_score = 0.55) {
  std::string text;
  for (const auto& g : SegmentGlyphs(roi)) {
    const auto c = ClassifyGlyph(g, templates, min_score);
    text.push_back(c ? *c : '?');
  }
  return text;
}

// --- stabilizer ------------------------------------------------------------
// Keeps the last kStabilizerWindow accepted elevations; rejects a sample that
// jumps more than max_jump_deg from the window median. A gap longer than
// kStaleSampleMs resets the window (stale data must not anchor rejection).

class ElevationStabilizer {
 public:
  explicit ElevationStabilizer(double max_jump_deg = kMaxJumpDeg)
      : max_jump_deg_(max_jump_deg) {}

  void Reset() {
    history_.clear();
    last_time_ms_.reset();
  }

  std::size_t size() const { return history_.size(); }

  // Returns the stabilized elevation (window median) or nullopt when the
  // sample is rejected (implausible jump).
  std::optional<double> Push(double time_ms, double elevation_deg) {
    if (!std::isfinite(time_ms) || !std::isfinite(elevation_deg)) {
      return std::nullopt;
    }
    if (last_time_ms_ && time_ms - *last_time_ms_ > kStaleSampleMs) {
      history_.clear();
    }
    last_time_ms_ = time_ms;
    if (!history_.empty()) {
      const double median = Median();
      if (std::fabs(elevation_deg - median) > max_jump_deg_) {
        return std::nullopt;
      }
    }
    history_.push_back(elevation_deg);
    while (history_.size() > kStabilizerWindow) history_.pop_front();
    return Median();
  }

 private:
  double Median() const {
    std::vector<double> v(history_.begin(), history_.end());
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n == 0) return 0.0;
    if (n % 2 == 1) return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
  }

  double max_jump_deg_;
  std::deque<double> history_;
  std::optional<double> last_time_ms_;
};

// --- READY gate ------------------------------------------------------------
// Enter: two in-window samples (|error| <= tolerance) whose time span is in
// [120ms, 500ms]. Exit: two consecutive samples beyond 2*tolerance, or any
// invalid/stale sample. Beep only on the entry edge.

enum class ReadyEvent { kNone, kEntered, kExited };

class ReadyGate {
 public:
  void Reset() {
    ready_ = false;
    window_start_ms_.reset();
    last_time_ms_.reset();
    out_count_ = 0;
  }

  bool ready() const { return ready_; }

  ReadyEvent Update(double time_ms, double elevation_deg, double target_deg,
                    double tolerance_deg) {
    if (!std::isfinite(time_ms) || !std::isfinite(elevation_deg) ||
        !std::isfinite(target_deg) || !(tolerance_deg > 0.0)) {
      return HandleInvalid(time_ms);
    }
    if (last_time_ms_ && time_ms - *last_time_ms_ > kStaleSampleMs) {
      HandleInvalid(time_ms);  // stale gap: full reset, current sample restarts
    }
    last_time_ms_ = time_ms;

    const double err = std::fabs(elevation_deg - target_deg);
    const bool in_window = err <= tolerance_deg;
    const bool far_out = err > kReadyExitFactor * tolerance_deg;

    if (!ready_) {
      if (in_window) {
        if (!window_start_ms_) {
          window_start_ms_ = time_ms;
        } else {
          const double span = time_ms - *window_start_ms_;
          if (span > kReadyMaxSpanMs) {
            window_start_ms_ = time_ms;  // window too old: restart
          } else if (span >= kReadyMinSpanMs) {
            ready_ = true;
            out_count_ = 0;
            window_start_ms_.reset();
            return ReadyEvent::kEntered;
          }
          // span < min: keep the first sample as window start and wait.
        }
      } else {
        window_start_ms_.reset();  // a miss breaks the hit streak
      }
      return ReadyEvent::kNone;
    }

    if (far_out) {
      ++out_count_;
      if (out_count_ >= 2) {
        ready_ = false;
        out_count_ = 0;
        return ReadyEvent::kExited;
      }
    } else {
      out_count_ = 0;
    }
    return ReadyEvent::kNone;
  }

  // OCR failure / out-of-range reading: reset the gate immediately.
  ReadyEvent OnInvalidSample(double time_ms) { return HandleInvalid(time_ms); }

 private:
  ReadyEvent HandleInvalid(double time_ms) {
    if (std::isfinite(time_ms)) last_time_ms_ = time_ms;
    const bool was_ready = ready_;
    ready_ = false;
    window_start_ms_.reset();
    out_count_ = 0;
    return was_ready ? ReadyEvent::kExited : ReadyEvent::kNone;
  }

  bool ready_ = false;
  std::optional<double> window_start_ms_;
  std::optional<double> last_time_ms_;
  int out_count_ = 0;
};

}  // namespace arcassist
