#pragma once
// Apex / Source cl_showpos automatic HUD locator.
//
// The compact binary templates below were derived offline from the supplied
// 2560x1440 Apex screenshot (pic.png, SHA-256 recorded in the verification
// record). Runtime never opens that image: it only receives a small GDI binary
// scan, finds the ANG label, and returns the first pitch token on that row.
// All helpers are portable C++17 so the real screenshot-derived fixture can be
// tested on macOS/Linux as well as Windows.

#include "arc_core.h"

#include <initializer_list>
#include <limits>
#include <numeric>

namespace arcassist {

constexpr int kApexHudTemplateWidth = 12;
constexpr int kApexHudTemplateHeight = 18;
constexpr int kApexHudDefaultThreshold = 150;
constexpr int kApexHudMaxChromaDelta = 64;

struct PositionedGlyph {
  BinaryGrid grid;
  int x = 0;
  int y = 0;
};

// Same 8-connected segmentation as SegmentGlyphs(), while retaining the
// component position needed to locate ANG and its following pitch token.
inline std::vector<PositionedGlyph> SegmentGlyphsWithBounds(
    const BinaryGrid& roi) {
  std::vector<PositionedGlyph> out;
  if (roi.width <= 0 || roi.height <= 0 ||
      roi.pixels.size() != static_cast<std::size_t>(roi.width) * roi.height) {
    return out;
  }

  struct Component {
    BinaryGrid grid;
    int x = 0;
    int y = 0;
  };
  std::vector<Component> components;
  std::vector<std::uint8_t> visited(
      static_cast<std::size_t>(roi.width) * roi.height, 0);
  std::vector<std::pair<int, int>> stack;

  for (int y = 0; y < roi.height; ++y) {
    for (int x = 0; x < roi.width; ++x) {
      const std::size_t start = static_cast<std::size_t>(y) * roi.width + x;
      if (!roi.At(x, y) || visited[start]) continue;

      int x0 = x, y0 = y, x1 = x, y1 = y;
      std::vector<std::pair<int, int>> cells;
      stack.clear();
      stack.push_back({x, y});
      visited[start] = 1;
      while (!stack.empty()) {
        const auto [cx, cy] = stack.back();
        stack.pop_back();
        cells.push_back({cx, cy});
        x0 = std::min(x0, cx);
        y0 = std::min(y0, cy);
        x1 = std::max(x1, cx);
        y1 = std::max(y1, cy);
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int nx = cx + dx;
            const int ny = cy + dy;
            if (nx < 0 || ny < 0 || nx >= roi.width || ny >= roi.height) {
              continue;
            }
            const std::size_t index =
                static_cast<std::size_t>(ny) * roi.width + nx;
            if (!roi.At(nx, ny) || visited[index]) continue;
            visited[index] = 1;
            stack.push_back({nx, ny});
          }
        }
      }

      BinaryGrid grid;
      grid.width = x1 - x0 + 1;
      grid.height = y1 - y0 + 1;
      grid.pixels.assign(static_cast<std::size_t>(grid.width) * grid.height, 0);
      for (const auto& [cx, cy] : cells) grid.Set(cx - x0, cy - y0, 1);
      components.push_back({std::move(grid), x0, y0});
    }
  }

  // Character order is horizontal. A decimal point sits below its digit, so
  // sorting by y first would incorrectly turn "-52.25" into "-5225.".
  std::sort(components.begin(), components.end(),
            [](const Component& a, const Component& b) {
              if (a.x != b.x) return a.x < b.x;
              return a.y < b.y;
            });
  out.reserve(components.size());
  for (auto& component : components) {
    out.push_back({std::move(component.grid), component.x, component.y});
  }
  return out;
}

inline BinaryGrid MakeApexHudGlyph(
    std::initializer_list<const char*> rows) {
  BinaryGrid grid;
  int width = 0;
  for (const char* row : rows) {
    if (row) width = std::max(width, static_cast<int>(std::char_traits<char>::length(row)));
  }
  grid.width = width;
  grid.height = static_cast<int>(rows.size());
  if (width <= 0 || grid.height <= 0) return grid;
  grid.pixels.assign(static_cast<std::size_t>(width) * grid.height, 0);
  int y = 0;
  for (const char* row : rows) {
    const std::size_t length = row ? std::char_traits<char>::length(row) : 0;
    for (std::size_t x = 0; x < length; ++x) {
      if (row[x] == '#') grid.Set(static_cast<int>(x), y, 1);
    }
    ++y;
  }
  return grid;
}

inline BinaryGrid CropApexHudForeground(const BinaryGrid& source) {
  if (source.width <= 0 || source.height <= 0 || source.pixels.empty()) {
    return {};
  }
  int x0 = source.width;
  int y0 = source.height;
  int x1 = -1;
  int y1 = -1;
  for (int y = 0; y < source.height; ++y) {
    for (int x = 0; x < source.width; ++x) {
      if (!source.At(x, y)) continue;
      x0 = std::min(x0, x);
      y0 = std::min(y0, y);
      x1 = std::max(x1, x);
      y1 = std::max(y1, y);
    }
  }
  if (x1 < x0 || y1 < y0) return {};

  BinaryGrid cropped;
  cropped.width = x1 - x0 + 1;
  cropped.height = y1 - y0 + 1;
  cropped.pixels.assign(
      static_cast<std::size_t>(cropped.width) * cropped.height, 0);
  for (int y = 0; y < cropped.height; ++y) {
    for (int x = 0; x < cropped.width; ++x) {
      cropped.Set(x, y, source.At(x0 + x, y0 + y));
    }
  }
  return cropped;
}

// Simulate the nearest-neighbour raster phase produced when the HUD is scaled
// with its parent screen. This preserves the low-resolution stroke losses that
// ordinary aspect-ratio normalization would otherwise hide (notably 75% HUD
// scale at 1920x1080).
inline BinaryGrid ScaleApexHudGlyphWithPhase(const BinaryGrid& glyph,
                                             int numerator, int denominator,
                                             int phase_x, int phase_y) {
  if (glyph.width <= 0 || glyph.height <= 0 || numerator <= 0 ||
      denominator <= 0 || phase_x < 0 || phase_y < 0 ||
      phase_x >= denominator || phase_y >= denominator) {
    return {};
  }
  // Keep the canvas dimensions divisible by the scale denominator. Otherwise
  // an integer output size would subtly change the sampling ratio and create
  // variants that cannot occur in a real 2560→1920/3200 raster.
  const int cells_x = std::max(8, (glyph.width + denominator * 5 +
                                   denominator - 1) / denominator);
  const int cells_y = std::max(8, (glyph.height + denominator * 5 +
                                   denominator - 1) / denominator);
  BinaryGrid source;
  source.width = denominator * cells_x;
  source.height = denominator * cells_y;
  source.pixels.assign(
      static_cast<std::size_t>(source.width) * source.height, 0);
  const int origin_x = denominator * 2 + phase_x;
  const int origin_y = denominator * 2 + phase_y;
  for (int y = 0; y < glyph.height; ++y) {
    for (int x = 0; x < glyph.width; ++x) {
      if (glyph.At(x, y)) source.Set(origin_x + x, origin_y + y, 1);
    }
  }

  BinaryGrid scaled;
  scaled.width = std::max(1, source.width * numerator / denominator);
  scaled.height = std::max(1, source.height * numerator / denominator);
  scaled.pixels.assign(
      static_cast<std::size_t>(scaled.width) * scaled.height, 0);
  for (int y = 0; y < scaled.height; ++y) {
    for (int x = 0; x < scaled.width; ++x) {
      scaled.Set(x, y, source.At(x * denominator / numerator,
                                 y * denominator / numerator));
    }
  }
  return CropApexHudForeground(scaled);
}

inline void AddApexHudScaleVariants(std::vector<GlyphTemplate>* templates,
                                    char id, const BinaryGrid& glyph) {
  if (!templates) return;
  // The supplied reference frame is 2560x1440.  These are the two practical
  // non-integer raster phases around it (1920x1080 and 3200x1800); each phase
  // stays local and is only a few dozen binary pixels.
  for (const auto& ratio : {std::pair<int, int>{3, 4},
                            std::pair<int, int>{5, 4}}) {
    for (int phase_y = 0; phase_y < ratio.second; ++phase_y) {
      for (int phase_x = 0; phase_x < ratio.second; ++phase_x) {
        const BinaryGrid scaled = ScaleApexHudGlyphWithPhase(
            glyph, ratio.first, ratio.second, phase_x, phase_y);
        if (scaled.width <= 0 || scaled.height <= 0) continue;
        templates->push_back(
            {id, NormalizeGlyph(scaled, kApexHudTemplateWidth,
                                kApexHudTemplateHeight)});
      }
    }
  }
}

inline void AddApexHudTemplate(std::vector<GlyphTemplate>* templates, char id,
                               std::initializer_list<const char*> rows) {
  if (!templates) return;
  BinaryGrid glyph = MakeApexHudGlyph(rows);
  if (glyph.width <= 0 || glyph.height <= 0) return;
  templates->push_back(
      {id, NormalizeGlyph(glyph, kApexHudTemplateWidth, kApexHudTemplateHeight)});
  AddApexHudScaleVariants(templates, id, glyph);
}

// The Source/Apex HUD uses a small outlined bitmap-like face.  These seed
// masks are taken from the supplied screenshot at a neutral-bright threshold;
// a standard 5x7 fallback is appended for digits not visible in that frame.
inline std::vector<GlyphTemplate> BuildApexHudTemplates() {
  std::vector<GlyphTemplate> result;
  result.reserve(48);

  // Exact neutral-bright masks sampled offline from pic.png. The screenshot
  // contains 0/1/2/3/4/5/7/8/9 across POS/ANG/VEL; only 6 and '+' use the
  // generic fallback because those glyphs do not occur in that reference frame.
  AddApexHudTemplate(&result, 'A', {
      "..####.", ".##.##.", ".##..##", "##...##", "#######", "##....#", ".#....#"});
  AddApexHudTemplate(&result, 'N', {
      "###...#", "###...#", "####..#", "##.##.#", "##..###", "##...##", ".#...##"});
  AddApexHudTemplate(&result, 'G', {
      ".######", ".#....#", "##.....", "##..###", "##...##", ".######", ".#####."});
  AddApexHudTemplate(&result, '-', {"####"});
  AddApexHudTemplate(&result, '.', {"#.", "##"});
  AddApexHudTemplate(&result, '.', {"##"});

  AddApexHudTemplate(&result, '0', {
      "######", "##..##", "###..#", "#.####", "#..###", "######", "#####."});
  AddApexHudTemplate(&result, '1', {
      "###..", "..##.", "..##.", "..##.", "..##.", "..##.", "#####"});
  AddApexHudTemplate(&result, '2', {
      ".######", ".#...##", ".....##", "...####", ".###...", "#######", ".######"});
  AddApexHudTemplate(&result, '2', {
      "#####.", "##..##", "#....#", "...###", "####..", "##....", "######"});
  AddApexHudTemplate(&result, '3', {
      "#####.", "##..##", "#....#", "..####", "....##", "#....#", "######"});
  AddApexHudTemplate(&result, '4', {
      "#....#", "#....#", "#....#", "######", ".#####", ".....#", ".....#"});
  AddApexHudTemplate(&result, '5', {
      ".######", "##.....", "######.", ".....##", ".#....#", ".######", ".#####."});
  AddApexHudTemplate(&result, '5', {
      "#####.", "#.....", "#.....", "######", ".....#", "#....#", "######"});
  AddApexHudTemplate(&result, '7', {
      "######", "....##", "....##", "..###.", ".##...", "##....", "#....."});
  AddApexHudTemplate(&result, '8', {
      "#####.", "##..##", "##...#", "####.#", "#..###", "#...##", "######"});
  AddApexHudTemplate(&result, '9', {
      ".######", "##....#", ".##..##", ".######", "......#", ".######", ".#####."});

  // Keep a normalized generic fallback for all fixed numeric glyphs. This
  // covers a font/HUD variant not represented by the screenshot seeds.
  for (const auto& item : BuildGlyphTemplates()) {
    result.push_back(
        {item.id, NormalizeGlyph(item.grid, kApexHudTemplateWidth,
                                 kApexHudTemplateHeight)});
  }
  return result;
}

struct HudGlyphGuess {
  char value = '?';
  double score = 0.0;
};

inline HudGlyphGuess GuessApexHudGlyph(
    const BinaryGrid& glyph, const std::vector<GlyphTemplate>& templates,
    std::string_view allowed) {
  HudGlyphGuess best;
  if (glyph.width <= 0 || glyph.height <= 0) return best;
  const BinaryGrid normalized =
      NormalizeGlyph(glyph, kApexHudTemplateWidth, kApexHudTemplateHeight);
  for (const auto& item : templates) {
    if (allowed.find(item.id) == std::string_view::npos ||
        item.grid.width != kApexHudTemplateWidth ||
        item.grid.height != kApexHudTemplateHeight) {
      continue;
    }
    const double score = 0.76 * OverlapScore(normalized, item.grid) +
                         0.24 * MatchScore(normalized, item.grid);
    if (score > best.score) {
      best.value = item.id;
      best.score = score;
    }
  }
  return best;
}

inline bool IsApexHudSignShape(const PositionedGlyph& glyph,
                                int reference_height) {
  // The Source minus is a thin horizontal bar. If it merges with a digit it
  // becomes a tall component and must not be accepted as a sign.
  return glyph.grid.width >= 2 &&
         glyph.grid.height <= std::max(2, reference_height / 2) &&
         glyph.grid.width >= glyph.grid.height * 2;
}

struct ApexHudPitchLocation {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  double confidence = 0.0;
  bool explicit_sign = false;
  std::string token;
};

struct ApexHudTokenRecognition {
  bool valid = false;
  double raw_angle = 0.0;
  double confidence = 0.0;
  bool explicit_sign = false;
  // False if a token begins/ends against the captured ROI edge. The pixels
  // can still parse, but a clipped leading sign/digit is not trustworthy for
  // an automatically located ARMED path.
  bool fully_framed = false;
  std::string token;
};

struct ApexHudThresholdLocation {
  ApexHudPitchLocation location;
  int threshold = kApexHudDefaultThreshold;
};

// cl_showpos prints the three angular values with exactly two fractional
// digits. Requiring that complete presentation form prevents a partial/nearby
// glyph (for example a lone `2` from POS) from becoming a successful auto
// location. This is stricter than the generic core parser by design.
inline std::optional<double> ParseCompleteApexHudPitchToken(
    std::string_view token) {
  const auto parsed = ParsePitchToken(token);
  if (!parsed) return std::nullopt;
  std::size_t dot = token.find('.');
  if (dot == std::string_view::npos || dot + 3 != token.size()) {
    return std::nullopt;
  }
  const std::size_t digits_begin =
      !token.empty() && (token[0] == '-' || token[0] == '+') ? 1 : 0;
  if (dot <= digits_begin || dot - digits_begin > 2) return std::nullopt;
  return parsed;
}

inline bool IsApexHudAutoPitchSignTrusted(double raw_angle,
                                          bool explicit_sign) {
  // A leading '-' can disappear entirely in a low-resolution raster. An
  // unsigned nonzero suffix such as `2.25` can likewise be carved from a
  // clipped high-angle token. cl_showpos emits two fractional digits, so only
  // an exact displayed zero is directionless enough to accept without +/-.
  return explicit_sign || std::fabs(raw_angle) < 0.005;
}

// F9 / Shift+F5 are explicit human recovery paths. A user may deliberately
// frame the visible token tightly, so they do not require surrounding blank
// pixels; they still require a complete parser result and an explicit sign for
// every nonzero value. The automatic ARMED path additionally requires padding
// to distinguish a complete field from a crop of a neighbouring field.
inline bool IsApexHudManualPitchTrusted(
    const ApexHudTokenRecognition& recognition) {
  return recognition.valid && IsApexHudAutoPitchSignTrusted(
                                  recognition.raw_angle,
                                  recognition.explicit_sign);
}

inline bool IsApexHudStrictPitchTrusted(
    const ApexHudTokenRecognition& recognition) {
  return recognition.fully_framed && IsApexHudManualPitchTrusted(recognition);
}

namespace detail {

inline int ApexHudRoot(std::vector<int>* parents, int index) {
  if (!parents || index < 0 || index >= static_cast<int>(parents->size())) {
    return -1;
  }
  int root = index;
  while ((*parents)[root] != root) root = (*parents)[root];
  while ((*parents)[index] != index) {
    const int next = (*parents)[index];
    (*parents)[index] = root;
    index = next;
  }
  return root;
}

// Scaling can remove a connecting row from a glyph (the 75% screenshot path
// splits `5` into two vertically adjacent components). The caller first
// restricts input to a confirmed text baseline, then this helper rejoins only
// fragments that substantially overlap in x and touch/are one pixel apart in y.
// Punctuation does not overlap its digit and therefore remains separate.
inline std::vector<PositionedGlyph> MergeApexHudGlyphFragments(
    const std::vector<PositionedGlyph>& input, int reference_height) {
  if (input.empty()) return {};
  std::vector<int> parents(input.size());
  std::iota(parents.begin(), parents.end(), 0);
  const int allowed_gap = std::max(1, reference_height / 3);
  for (int i = 0; i < static_cast<int>(input.size()); ++i) {
    for (int j = i + 1; j < static_cast<int>(input.size()); ++j) {
      const PositionedGlyph& a = input[static_cast<std::size_t>(i)];
      const PositionedGlyph& b = input[static_cast<std::size_t>(j)];
      const int overlap_left = std::max(a.x, b.x);
      const int overlap_right = std::min(a.x + a.grid.width - 1,
                                         b.x + b.grid.width - 1);
      const int overlap = overlap_right - overlap_left + 1;
      const int minimum_width = std::min(a.grid.width, b.grid.width);
      const int vertical_gap =
          std::max(a.y, b.y) -
          std::min(a.y + a.grid.height - 1, b.y + b.grid.height - 1) - 1;
      if (overlap < std::max(1, (minimum_width + 1) / 2) ||
          vertical_gap > allowed_gap) {
        continue;
      }
      const int a_root = ApexHudRoot(&parents, i);
      const int b_root = ApexHudRoot(&parents, j);
      if (a_root >= 0 && b_root >= 0 && a_root != b_root) {
        parents[static_cast<std::size_t>(b_root)] = a_root;
      }
    }
  }

  std::vector<PositionedGlyph> merged;
  merged.reserve(input.size());
  for (int group = 0; group < static_cast<int>(input.size()); ++group) {
    if (ApexHudRoot(&parents, group) != group) continue;
    int x0 = std::numeric_limits<int>::max();
    int y0 = std::numeric_limits<int>::max();
    int x1 = -1;
    int y1 = -1;
    for (int index = 0; index < static_cast<int>(input.size()); ++index) {
      if (ApexHudRoot(&parents, index) != group) continue;
      const PositionedGlyph& glyph = input[static_cast<std::size_t>(index)];
      x0 = std::min(x0, glyph.x);
      y0 = std::min(y0, glyph.y);
      x1 = std::max(x1, glyph.x + glyph.grid.width - 1);
      y1 = std::max(y1, glyph.y + glyph.grid.height - 1);
    }
    if (x1 < x0 || y1 < y0) continue;
    BinaryGrid grid;
    grid.width = x1 - x0 + 1;
    grid.height = y1 - y0 + 1;
    grid.pixels.assign(static_cast<std::size_t>(grid.width) * grid.height, 0);
    for (int index = 0; index < static_cast<int>(input.size()); ++index) {
      if (ApexHudRoot(&parents, index) != group) continue;
      const PositionedGlyph& glyph = input[static_cast<std::size_t>(index)];
      for (int y = 0; y < glyph.grid.height; ++y) {
        for (int x = 0; x < glyph.grid.width; ++x) {
          if (glyph.grid.At(x, y)) grid.Set(glyph.x - x0 + x,
                                             glyph.y - y0 + y, 1);
        }
      }
    }
    merged.push_back({std::move(grid), x0, y0});
  }
  std::sort(merged.begin(), merged.end(),
            [](const PositionedGlyph& left, const PositionedGlyph& right) {
              return left.x == right.x ? left.y < right.y : left.x < right.x;
            });
  return merged;
}

}  // namespace detail

// Recognize an already-localized pitch ROI with screenshot-derived Apex
// templates. A normal automatic ROI may be padded to the minimum 72x18 size,
// so it can contain pieces of the neighbouring ANG values or HUD rows. Rather
// than treating every component as one token, enumerate complete two-decimal
// tokens on each text baseline and choose the one closest to the ROI centre.
// The locator creates a centred ROI, so this is deterministic while remaining
// strict about every character, decimal point, and leading sign shape.
inline ApexHudTokenRecognition RecognizeApexHudPitchToken(
    const BinaryGrid& roi, const std::vector<GlyphTemplate>& templates,
    double min_score = 0.42) {
  ApexHudTokenRecognition result;
  const std::vector<PositionedGlyph> raw_components =
      SegmentGlyphsWithBounds(roi);
  if (raw_components.empty()) return result;

  int tallest = 0;
  for (const auto& component : raw_components) {
    tallest = std::max(tallest, component.grid.height);
  }
  if (tallest <= 0) return result;

  std::vector<int> baselines;
  baselines.reserve(raw_components.size());
  for (const auto& component : raw_components) {
    // A split 75%-scale glyph can be one pixel shorter than the line's intact
    // digits. Use intact/tall pieces as baseline seeds, then add its fragments.
    if (component.grid.height + 1 >= tallest) {
      baselines.push_back(2 * component.y + component.grid.height);
    }
  }
  if (baselines.empty()) return result;
  std::sort(baselines.begin(), baselines.end());
  baselines.erase(std::unique(baselines.begin(), baselines.end()),
                  baselines.end());

  struct Candidate {
    ApexHudTokenRecognition recognition;
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
  };
  std::optional<Candidate> best;
  const int roi_center_x2 = roi.width;
  const int roi_center_y2 = roi.height;

  for (const int baseline : baselines) {
    const int baseline_tolerance = std::max(3, tallest);
    std::vector<PositionedGlyph> line;
    line.reserve(raw_components.size());
    for (const auto& component : raw_components) {
      const int center2 = 2 * component.y + component.grid.height;
      if (std::abs(center2 - baseline) <= baseline_tolerance) {
        line.push_back(component);
      }
    }
    if (line.empty()) continue;
    int reference_height = 0;
    for (const auto& component : line) {
      reference_height = std::max(reference_height, component.grid.height);
    }
    const std::vector<PositionedGlyph> merged =
        detail::MergeApexHudGlyphFragments(line, reference_height);
    if (merged.empty() || merged.size() > 16) continue;

    struct Glyph {
      const PositionedGlyph* glyph = nullptr;
      char value = '?';
      double score = 0.0;
    };
    std::vector<Glyph> glyphs;
    glyphs.reserve(merged.size());
    for (const auto& component : merged) {
      const HudGlyphGuess guess = GuessApexHudGlyph(
          component.grid, templates, "-+.0123456789");
      if (guess.score >= min_score) {
        glyphs.push_back({&component, guess.value, guess.score});
      }
    }
    std::sort(glyphs.begin(), glyphs.end(),
              [](const Glyph& left, const Glyph& right) {
                return left.glyph->x == right.glyph->x
                           ? left.glyph->y < right.glyph->y
                           : left.glyph->x < right.glyph->x;
              });

    const int maximum_gap = std::max(4, (reference_height * 5) / 4);
    for (std::size_t start = 0; start < glyphs.size(); ++start) {
      // A padded ROI may contain an entire token, but its centre is not
      // necessarily the token centre. Never start another parse in the middle
      // of a contiguous run: otherwise `-52.25` can yield the plausible but
      // directionally unrelated suffix `2.25` when the ROI is right-biased.
      if (start > 0) {
        const Glyph& previous = glyphs[start - 1];
        const int preceding_gap = glyphs[start].glyph->x -
                                  (previous.glyph->x +
                                   previous.glyph->grid.width);
        if (preceding_gap <= maximum_gap) continue;
      }
      if (glyphs[start].value == '.' || glyphs[start].value == '+' ||
          glyphs[start].value == '-') {
        if (glyphs[start].value == '.' ||
            !IsApexHudSignShape(*glyphs[start].glyph, reference_height)) {
          continue;
        }
      }
      std::string token;
      double score_sum = 0.0;
      int x0 = glyphs[start].glyph->x;
      int y0 = glyphs[start].glyph->y;
      int x1 = x0 + glyphs[start].glyph->grid.width - 1;
      int y1 = y0 + glyphs[start].glyph->grid.height - 1;
      for (std::size_t end = start; end < glyphs.size(); ++end) {
        if (end > start) {
          const Glyph& previous = glyphs[end - 1];
          const int gap = glyphs[end].glyph->x -
                          (previous.glyph->x + previous.glyph->grid.width);
          if (gap < 0 || gap > maximum_gap) break;
          if (glyphs[end].value == '-' || glyphs[end].value == '+') break;
        }
        token.push_back(glyphs[end].value);
        score_sum += glyphs[end].score;
        x0 = std::min(x0, glyphs[end].glyph->x);
        y0 = std::min(y0, glyphs[end].glyph->y);
        x1 = std::max(x1, glyphs[end].glyph->x +
                          glyphs[end].glyph->grid.width - 1);
        y1 = std::max(y1, glyphs[end].glyph->y +
                          glyphs[end].glyph->grid.height - 1);
        const auto raw_angle = ParseCompleteApexHudPitchToken(token);
        if (!raw_angle) continue;
        // A complete presentation must also end at a token boundary. A
        // following digit or decimal within the same run means this parse is
        // only a prefix of a longer/ambiguous sequence, not a standalone
        // cl_showpos value. A valid +/- starts the next angle column.
        if (end + 1 < glyphs.size()) {
          const Glyph& next = glyphs[end + 1];
          const int following_gap = next.glyph->x -
                                    (glyphs[end].glyph->x +
                                     glyphs[end].glyph->grid.width);
          const bool next_is_sign =
              (next.value == '-' || next.value == '+') &&
              IsApexHudSignShape(*next.glyph, reference_height);
          if (following_gap <= maximum_gap && !next_is_sign) continue;
        }
        const bool explicit_sign = token[0] == '-' || token[0] == '+';
        if (explicit_sign &&
            !IsApexHudSignShape(*glyphs[start].glyph, reference_height)) {
          continue;
        }
        Candidate candidate;
        candidate.recognition.valid = true;
        candidate.recognition.raw_angle = *raw_angle;
        candidate.recognition.confidence =
            score_sum / static_cast<double>(token.size());
        candidate.recognition.explicit_sign = explicit_sign;
        // A locator ROI normally includes 2–4 pixels of horizontal padding.
        // If the selected token touches an edge, it may be a crop of another
        // pitch (for example the `52.25` suffix of `-52.25`), so the strict
        // automatic path must not use it even if its shape scores well.
        candidate.recognition.fully_framed =
            x0 >= 2 && x1 + 2 < roi.width && y0 >= 1 && y1 + 1 < roi.height;
        candidate.recognition.token = token;
        candidate.x0 = x0;
        candidate.y0 = y0;
        candidate.x1 = x1;
        candidate.y1 = y1;

        const int candidate_center_x2 = candidate.x0 + candidate.x1 + 1;
        const int candidate_center_y2 = candidate.y0 + candidate.y1 + 1;
        const int candidate_vertical_distance =
            std::abs(candidate_center_y2 - roi_center_y2);
        const int candidate_horizontal_distance =
            std::abs(candidate_center_x2 - roi_center_x2);
        if (!best) {
          best = std::move(candidate);
          continue;
        }
        const int best_center_x2 = best->x0 + best->x1 + 1;
        const int best_center_y2 = best->y0 + best->y1 + 1;
        const int best_vertical_distance =
            std::abs(best_center_y2 - roi_center_y2);
        const int best_horizontal_distance =
            std::abs(best_center_x2 - roi_center_x2);
        if (candidate_vertical_distance < best_vertical_distance ||
            (candidate_vertical_distance == best_vertical_distance &&
             candidate_horizontal_distance < best_horizontal_distance) ||
            (candidate_vertical_distance == best_vertical_distance &&
             candidate_horizontal_distance == best_horizontal_distance &&
             candidate.recognition.confidence > best->recognition.confidence)) {
          best = std::move(candidate);
        }
      }
    }
  }
  return best ? best->recognition : result;
}

inline ApexHudTokenRecognition RecognizeApexHudPitchToken(
    const BinaryGrid& roi, double min_score = 0.42) {
  return RecognizeApexHudPitchToken(roi, BuildApexHudTemplates(), min_score);
}

namespace detail {

struct ApexHudCandidate {
  const PositionedGlyph* glyph = nullptr;
  char value = '?';
  double score = 0.0;
};

inline bool SimilarHudLine(const PositionedGlyph& a, const PositionedGlyph& b,
                           int reference_height) {
  const int ay2 = 2 * a.y + a.grid.height;
  const int by2 = 2 * b.y + b.grid.height;
  return std::abs(ay2 - by2) <= std::max(3, reference_height);
}

inline bool AdjacentHudGlyphs(const PositionedGlyph& left,
                              const PositionedGlyph& right,
                              int reference_height) {
  const int gap = right.x - (left.x + left.grid.width);
  return gap >= 0 && gap <= std::max(4, reference_height * 2);
}

// cl_showpos uses a fixed left text column: its line labels begin at the same
// small x coordinate. A false A/N/G sequence inside POS/VEL values is never a
// legitimate ANG label. Permit enough slack for the capture crop and HUD
// scaling but require the A to be a leading-column glyph.
inline bool IsLeadingHudLabelGlyph(
    const PositionedGlyph& glyph,
    const std::vector<PositionedGlyph>& components, int reference_height) {
  const int maximum_left = std::max(24, reference_height * 4);
  if (glyph.x > maximum_left) return false;
  const int column_begin = std::max(0, glyph.x - std::max(2, reference_height));
  for (const auto& other : components) {
    if (other.x >= glyph.x || other.x < column_begin) continue;
    if (SimilarHudLine(other, glyph, reference_height)) return false;
  }
  return true;
}

inline bool HasApexHudLabelColon(
    const std::vector<PositionedGlyph>& components,
    const PositionedGlyph& label_end, int reference_height) {
  // In the reference HUD `ANG:` has a real two-dot colon immediately after G.
  // Requiring it prevents `POS` digits that merely resemble A/N/G from
  // becoming a successful automatic location. The colon may be one merged
  // 2x2 component or two 1–2px components after scaling.
  const int label_center2 = 2 * label_end.y + label_end.grid.height;
  const int x0 = label_end.x + label_end.grid.width;
  const int x1 = x0 + std::max(5, reference_height * 2);
  std::vector<const PositionedGlyph*> dots;
  for (const auto& component : components) {
    const int maximum_dot_size = std::max(3, reference_height / 2);
    if (component.x < x0 || component.x > x1 ||
        component.grid.width > maximum_dot_size ||
        component.grid.height > maximum_dot_size) {
      continue;
    }
    const int center2 = 2 * component.y + component.grid.height;
    if (std::abs(center2 - label_center2) >
        std::max(3, reference_height)) {
      continue;
    }
    dots.push_back(&component);
  }
  if (dots.empty()) return false;
  for (const PositionedGlyph* dot : dots) {
    // At 2x a minus can have the same small bounding box as a merged colon.
    // Do not let that following pitch sign satisfy a missing-label-colon;
    // separate one-pixel colon dots remain valid and are paired below.
    if (dot->grid.height >= 2 &&
        !IsApexHudSignShape(*dot, reference_height)) {
      return true;  // merged 2-dot component
    }
    for (const PositionedGlyph* other : dots) {
      if (other == dot || other->x != dot->x) continue;
      const int dy = std::abs(other->y - dot->y);
      if (dy >= 1 && dy <= std::max(4, reference_height)) return true;
    }
  }
  return false;
}

}  // namespace detail

// Locate the first valid numeric token following an ANG label.  The caller
// supplies only a one-shot upper-left binary scan.  Returned coordinates are
// relative to that scan and include enough padding for the normal tiny ROI.
inline std::optional<ApexHudPitchLocation> LocateApexHudPitch(
    const BinaryGrid& scan, const std::vector<GlyphTemplate>& templates,
    double min_label_score = 0.45, double min_numeric_score = 0.42) {
  const std::vector<PositionedGlyph> components = SegmentGlyphsWithBounds(scan);
  if (components.size() < 5) return std::nullopt;

  std::vector<detail::ApexHudCandidate> labels_a;
  std::vector<detail::ApexHudCandidate> labels_n;
  std::vector<detail::ApexHudCandidate> labels_g;
  for (const auto& component : components) {
    if (component.grid.width < 2 || component.grid.height < 4 ||
        component.grid.width > 80 || component.grid.height > 80) {
      continue;
    }
    const HudGlyphGuess guess = GuessApexHudGlyph(component.grid, templates, "ANG");
    if (guess.score < min_label_score) continue;
    if (guess.value == 'A' && detail::IsLeadingHudLabelGlyph(
                                  component, components,
                                  component.grid.height)) {
      labels_a.push_back({&component, 'A', guess.score});
    }
    if (guess.value == 'N') labels_n.push_back({&component, 'N', guess.score});
    if (guess.value == 'G') labels_g.push_back({&component, 'G', guess.score});
  }

  std::optional<ApexHudPitchLocation> best;
  for (const auto& a : labels_a) {
    for (const auto& n : labels_n) {
      const int reference_height = std::max(a.glyph->grid.height, n.glyph->grid.height);
      if (n.glyph->x <= a.glyph->x ||
          !detail::SimilarHudLine(*a.glyph, *n.glyph, reference_height) ||
          !detail::AdjacentHudGlyphs(*a.glyph, *n.glyph, reference_height)) {
        continue;
      }
      for (const auto& g : labels_g) {
        const int label_height = std::max(
            {a.glyph->grid.height, n.glyph->grid.height, g.glyph->grid.height});
        if (g.glyph->x <= n.glyph->x ||
            !detail::SimilarHudLine(*a.glyph, *g.glyph, label_height) ||
            !detail::AdjacentHudGlyphs(*n.glyph, *g.glyph, label_height) ||
            !detail::HasApexHudLabelColon(components, *g.glyph,
                                           label_height)) {
          continue;
        }

        const int label_center2 = 2 * a.glyph->y + a.glyph->grid.height;
        const int minimum_token_x = g.glyph->x + g.glyph->grid.width +
                                    std::max(4, label_height * 2);
        const int line_tolerance = std::max(4, label_height + 2);

        // Filter to the confirmed ANG baseline *before* joining split glyph
        // fragments. At 75% scale a `5` may be two vertically adjacent
        // components; if the whole scan were merged first, a VEL character
        // directly below it could become the same component.
        std::vector<PositionedGlyph> numeric_line_components;
        numeric_line_components.reserve(components.size());
        for (const auto& component : components) {
          if (component.x < minimum_token_x ||
              component.grid.width > label_height * 2 ||
              component.grid.height > label_height * 2) {
            continue;
          }
          const int center2 = 2 * component.y + component.grid.height;
          if (std::abs(center2 - label_center2) > line_tolerance) continue;
          numeric_line_components.push_back(component);
        }
        const std::vector<PositionedGlyph> merged_components =
            detail::MergeApexHudGlyphFragments(numeric_line_components,
                                                label_height);
        std::vector<detail::ApexHudCandidate> numeric;
        for (const auto& component : merged_components) {
          if (component.grid.width > label_height * 2 ||
              component.grid.height > label_height * 2) {
            continue;
          }
          const int center2 = 2 * component.y + component.grid.height;
          if (std::abs(center2 - label_center2) > line_tolerance) continue;
          const HudGlyphGuess guess = GuessApexHudGlyph(
              component.grid, templates, "-+.0123456789");
          if (guess.score >= min_numeric_score) {
            numeric.push_back({&component, guess.value, guess.score});
          }
        }
        std::sort(numeric.begin(), numeric.end(),
                  [](const detail::ApexHudCandidate& left,
                     const detail::ApexHudCandidate& right) {
                    return left.glyph->x < right.glyph->x;
                  });
        if (numeric.empty()) continue;

        std::vector<detail::ApexHudCandidate> token_glyphs;
        const int maximum_gap = std::max(4, (label_height * 5) / 4);
        for (const auto& candidate : numeric) {
          if (token_glyphs.empty()) {
            if (candidate.value == '.' || candidate.value == '+' ||
                candidate.value == '-') {
              // A sign may start a token; a decimal point cannot.
              if (candidate.value == '.') continue;
            }
            token_glyphs.push_back(candidate);
            continue;
          }
          const auto& previous = token_glyphs.back();
          const int gap = candidate.glyph->x -
                          (previous.glyph->x + previous.glyph->grid.width);
          if (gap > maximum_gap) break;  // next cl_showpos column/token
          token_glyphs.push_back(candidate);
        }
        if (token_glyphs.empty()) continue;

        std::string token;
        double score_sum = a.score + n.score + g.score;
        int x0 = token_glyphs.front().glyph->x;
        int y0 = token_glyphs.front().glyph->y;
        int x1 = x0 + token_glyphs.front().glyph->grid.width - 1;
        int y1 = y0 + token_glyphs.front().glyph->grid.height - 1;
        for (const auto& candidate : token_glyphs) {
          token.push_back(candidate.value);
          score_sum += candidate.score;
          x0 = std::min(x0, candidate.glyph->x);
          y0 = std::min(y0, candidate.glyph->y);
          x1 = std::max(x1, candidate.glyph->x + candidate.glyph->grid.width - 1);
          y1 = std::max(y1, candidate.glyph->y + candidate.glyph->grid.height - 1);
        }
        const auto pitch = ParseCompleteApexHudPitchToken(token);
        if (!pitch) continue;
        const bool explicit_sign = token[0] == '-' || token[0] == '+';
        if (!IsApexHudAutoPitchSignTrusted(*pitch, explicit_sign)) continue;
        if (explicit_sign &&
            !IsApexHudSignShape(*token_glyphs.front().glyph, label_height)) {
          continue;
        }

        const int padding_x = std::max(2, (label_height + 1) / 2);
        const int padding_y = std::max(2, (label_height + 2) / 3);
        ApexHudPitchLocation found;
        found.x = std::max(0, x0 - padding_x);
        found.y = std::max(0, y0 - padding_y);
        found.width = std::min(scan.width - found.x, x1 - found.x + 1 + padding_x);
        found.height = std::min(scan.height - found.y, y1 - found.y + 1 + padding_y);
        found.explicit_sign = token[0] == '-' || token[0] == '+';
        found.token = std::move(token);
        found.confidence = score_sum /
                           static_cast<double>(3 + token_glyphs.size());
        if (found.width <= 0 || found.height <= 0) continue;
        if (!best || found.confidence > best->confidence) best = std::move(found);
      }
    }
  }
  return best;
}

inline bool AreApexHudPitchLocationsCompatible(
    const ApexHudPitchLocation& left, const ApexHudPitchLocation& right) {
  if (left.token != right.token ||
      left.explicit_sign != right.explicit_sign) {
    return false;
  }
  // Threshold changes may add/remove a one-pixel outline, but candidates from
  // the same ANG field must still substantially overlap. This rejects a
  // high-confidence false label on another HUD row instead of arbitrarily
  // selecting whichever threshold happened to score it highest.
  const int overlap_left = std::max(left.x, right.x);
  const int overlap_top = std::max(left.y, right.y);
  const int overlap_right = std::min(left.x + left.width,
                                     right.x + right.width);
  const int overlap_bottom = std::min(left.y + left.height,
                                      right.y + right.height);
  const int overlap_width = std::max(0, overlap_right - overlap_left);
  const int overlap_height = std::max(0, overlap_bottom - overlap_top);
  return overlap_width * 2 >= std::min(left.width, right.width) &&
         overlap_height * 2 >= std::min(left.height, right.height);
}

// Choose the most confident strict `ANG:` candidate from masks generated from
// one already-captured frame. The caller can vary only a local brightness
// threshold without performing another screen capture. A second successful
// threshold must corroborate the same token in the same HUD region; otherwise
// the one-shot result is ambiguous and must fail closed.
inline std::optional<ApexHudThresholdLocation> LocateApexHudPitchAcrossMasks(
    const std::vector<BinaryGrid>& masks, const std::vector<int>& thresholds,
    const std::vector<GlyphTemplate>& templates,
    double min_label_score = 0.45, double min_numeric_score = 0.42) {
  if (masks.size() != thresholds.size()) return std::nullopt;
  std::vector<ApexHudPitchLocation> corroborated;
  std::optional<ApexHudThresholdLocation> best;
  for (std::size_t i = 0; i < masks.size(); ++i) {
    const auto located = LocateApexHudPitch(masks[i], templates,
                                             min_label_score,
                                             min_numeric_score);
    if (!located) continue;
    for (const auto& previous : corroborated) {
      if (!AreApexHudPitchLocationsCompatible(previous, *located)) {
        return std::nullopt;
      }
    }
    corroborated.push_back(*located);
    if (!best || located->confidence > best->location.confidence) {
      best = ApexHudThresholdLocation{*located, thresholds[i]};
    }
  }
  return best;
}

inline std::optional<ApexHudPitchLocation> LocateApexHudPitch(
    const BinaryGrid& scan, double min_label_score = 0.45,
    double min_numeric_score = 0.42) {
  return LocateApexHudPitch(scan, BuildApexHudTemplates(), min_label_score,
                            min_numeric_score);
}

}  // namespace arcassist
