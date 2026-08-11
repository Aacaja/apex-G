// Apex Arc Assist
// Native Win32, pixel-only, event-driven helper for the user's high-arc formula.
// Runtime path: tiny GDI desktop ROI -> binary glyph templates -> local math ->
// this process's own overlay/beep. No game-process access and no GPU/video SDK.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include "arc_core.h"
#include "roi_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <iomanip>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using arcassist::BinaryGrid;
using arcassist::GlyphTemplate;
using arcassist::PitchConvention;
using arcassist::ReadyEvent;

constexpr wchar_t kOverlayClass[] = L"ApexArcAssistOverlay";
constexpr wchar_t kRoiClass[] = L"ApexArcAssistRoi";
constexpr wchar_t kTitle[] = L"Apex Arc Assist";
constexpr UINT_PTR kCaptureTimer = 1;
constexpr int kOverlayWidth = 610;
constexpr int kOverlayHeight = 226;
constexpr int kNormW = 12;
constexpr int kNormH = 18;
constexpr std::uint32_t kTemplateMagic = 0x31544141U;  // AAT1

constexpr UINT kFarIntervalMs = 200;       // 5 Hz
constexpr UINT kNearIntervalMs = 83;       // about 12 Hz
constexpr UINT kReadyIntervalMs = 333;     // 3 Hz
constexpr UINT kLost1IntervalMs = 250;     // 4 Hz
constexpr UINT kLost2IntervalMs = 500;     // 2 Hz
constexpr UINT kLost3IntervalMs = 1000;    // 1 Hz
constexpr double kNearBandDeg = 8.0;

constexpr int HK_TRAIN = 10;
constexpr int HK_PAUSE = 11;
constexpr int HK_SIGN = 12;
constexpr int HK_DISTANCE = 13;
constexpr int HK_CALIBRATE = 14;
constexpr int HK_SOUND = 15;
constexpr int HK_QUIT = 16;
constexpr int HK_TEXT_DIGIT_BASE = 100;
constexpr int HK_TEXT_NUMPAD_BASE = 110;
constexpr int HK_TEXT_PERIOD = 120;
constexpr int HK_TEXT_DECIMAL = 121;
constexpr int HK_TEXT_MINUS = 122;
constexpr int HK_TEXT_PLUS = 123;
constexpr int HK_TEXT_BACK = 124;
constexpr int HK_TEXT_ENTER = 125;
constexpr int HK_TEXT_ESCAPE = 126;
constexpr int HK_CAL_LEFT = 140;
constexpr int HK_CAL_RIGHT = 141;
constexpr int HK_CAL_UP = 142;
constexpr int HK_CAL_DOWN = 143;
constexpr int HK_CAL_SHIFT_LEFT = 144;
constexpr int HK_CAL_SHIFT_RIGHT = 145;
constexpr int HK_CAL_SHIFT_UP = 146;
constexpr int HK_CAL_SHIFT_DOWN = 147;
constexpr int HK_CAL_CTRL_LEFT = 148;
constexpr int HK_CAL_CTRL_RIGHT = 149;
constexpr int HK_CAL_CTRL_UP = 150;
constexpr int HK_CAL_CTRL_DOWN = 151;
constexpr int HK_CAL_THRESHOLD_UP = 152;
constexpr int HK_CAL_THRESHOLD_DOWN = 153;
constexpr int HK_CAL_ENTER = 154;
constexpr int HK_CAL_ESCAPE = 155;
constexpr int HK_CAL_SHRINK = 156;
constexpr int HK_CAL_SHRINK_NUMPAD = 157;
constexpr int HK_CAL_GROW = 158;
constexpr int HK_CAL_GROW_SHIFT = 159;
constexpr int HK_CAL_GROW_NUMPAD = 160;
constexpr int HK_CAL_COMPACT = 161;

using Roi = arcassist::RoiGeometry;

enum class UiMode { kNormal, kDistance, kCalibrate, kTrainLabel };

std::wstring ModuleSiblingPath(const wchar_t* leaf) {
  std::array<wchar_t, 32768> path{};
  const DWORD n = GetModuleFileNameW(nullptr, path.data(),
                                     static_cast<DWORD>(path.size()));
  std::wstring result(path.data(), n);
  const std::size_t slash = result.find_last_of(L"\\/");
  if (slash != std::wstring::npos) result.resize(slash + 1);
  else result.clear();
  result += leaf;
  return result;
}

bool WriteIniInt(const std::wstring& path, const wchar_t* section,
                 const wchar_t* key, int value) {
  wchar_t buffer[32]{};
  _snwprintf_s(buffer, _TRUNCATE, L"%d", value);
  return WritePrivateProfileStringW(section, key, buffer, path.c_str()) != FALSE;
}

bool WriteIniDouble(const std::wstring& path, const wchar_t* section,
                    const wchar_t* key, double value) {
  wchar_t buffer[64]{};
  _snwprintf_s(buffer, _TRUNCATE, L"%.6f", value);
  return WritePrivateProfileStringW(section, key, buffer, path.c_str()) != FALSE;
}

int ReadIniInt(const std::wstring& path, const wchar_t* section,
               const wchar_t* key, int fallback) {
  return static_cast<int>(GetPrivateProfileIntW(section, key, fallback,
                                                 path.c_str()));
}

double ReadIniDouble(const std::wstring& path, const wchar_t* section,
                     const wchar_t* key, double fallback) {
  wchar_t buffer[64]{};
  wchar_t fallback_text[64]{};
  _snwprintf_s(fallback_text, _TRUNCATE, L"%.6f", fallback);
  GetPrivateProfileStringW(section, key, fallback_text, buffer,
                           static_cast<DWORD>(std::size(buffer)), path.c_str());
  wchar_t* end = nullptr;
  const double value = std::wcstod(buffer, &end);
  return end != buffer && std::isfinite(value) ? value : fallback;
}

class QpcClock {
 public:
  QpcClock() { QueryPerformanceFrequency(&frequency_); }

  std::int64_t NowTicks() const {
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
  }

  double ToMicroseconds(std::int64_t ticks) const {
    return 1000000.0 * static_cast<double>(ticks) /
           static_cast<double>(frequency_.QuadPart);
  }

  double ToMilliseconds(std::int64_t ticks) const {
    return 1000.0 * static_cast<double>(ticks) /
           static_cast<double>(frequency_.QuadPart);
  }

 private:
  LARGE_INTEGER frequency_{};
};

class RollingStats {
 public:
  void Add(double microseconds) {
    values_[next_] = microseconds;
    next_ = (next_ + 1) % values_.size();
    count_ = std::min(count_ + 1, values_.size());
  }

  std::pair<double, double> MedianAndP95() const {
    if (count_ == 0) return {0.0, 0.0};
    std::vector<double> copy(values_.begin(), values_.begin() + count_);
    std::sort(copy.begin(), copy.end());
    const std::size_t median = copy.size() / 2;
    const std::size_t p95 = std::min(
        copy.size() - 1,
        static_cast<std::size_t>(std::ceil(copy.size() * 0.95)) - 1);
    return {copy[median], copy[p95]};
  }

 private:
  std::array<double, 120> values_{};
  std::size_t next_ = 0;
  std::size_t count_ = 0;
};

class CaptureSurface {
 public:
  ~CaptureSurface() { Reset(); }

  void Reset() {
    if (memory_dc_) {
      if (old_bitmap_) SelectObject(memory_dc_, old_bitmap_);
      if (bitmap_) DeleteObject(bitmap_);
      DeleteDC(memory_dc_);
    }
    if (screen_dc_) ReleaseDC(nullptr, screen_dc_);
    screen_dc_ = nullptr;
    memory_dc_ = nullptr;
    bitmap_ = nullptr;
    old_bitmap_ = nullptr;
    bits_ = nullptr;
    width_ = 0;
    height_ = 0;
  }

  bool Ensure(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (screen_dc_ && memory_dc_ && bitmap_ && width == width_ &&
        height == height_) {
      return true;
    }
    Reset();
    screen_dc_ = GetDC(nullptr);
    if (!screen_dc_) return false;
    memory_dc_ = CreateCompatibleDC(screen_dc_);
    if (!memory_dc_) {
      Reset();
      return false;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height;  // top-down
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    bitmap_ = CreateDIBSection(screen_dc_, &info, DIB_RGB_COLORS,
                               reinterpret_cast<void**>(&bits_), nullptr, 0);
    if (!bitmap_ || !bits_) {
      Reset();
      return false;
    }
    old_bitmap_ = SelectObject(memory_dc_, bitmap_);
    if (!old_bitmap_ || old_bitmap_ == HGDI_ERROR) {
      old_bitmap_ = nullptr;
      Reset();
      return false;
    }
    width_ = width;
    height_ = height;
    return true;
  }

  bool Grab(const Roi& roi, int threshold, BinaryGrid* out) {
    if (!out || !Ensure(roi.w, roi.h)) return false;
    if (!BitBlt(memory_dc_, 0, 0, roi.w, roi.h, screen_dc_, roi.x, roi.y,
                SRCCOPY)) {
      return false;
    }

    out->width = roi.w;
    out->height = roi.h;
    out->pixels.assign(static_cast<std::size_t>(roi.w) * roi.h, 0);
    for (int y = 0; y < roi.h; ++y) {
      for (int x = 0; x < roi.w; ++x) {
        const std::size_t i = static_cast<std::size_t>(y) * roi.w + x;
        const std::uint8_t b = bits_[4 * i + 0];
        const std::uint8_t g = bits_[4 * i + 1];
        const std::uint8_t r = bits_[4 * i + 2];
        const int high = std::max({static_cast<int>(r), static_cast<int>(g),
                                   static_cast<int>(b)});
        const int low = std::min({static_cast<int>(r), static_cast<int>(g),
                                  static_cast<int>(b)});
        // cl_showpos text is bright and near-neutral. The second clause keeps
        // fully white antialiasing even on slightly coloured backgrounds.
        const bool bright = high >= threshold;
        const bool neutral = (high - low) <= 92 || high >= 248;
        out->pixels[i] = bright && neutral ? 1 : 0;
      }
    }
    return true;
  }

 private:
  HDC screen_dc_ = nullptr;
  HDC memory_dc_ = nullptr;
  HBITMAP bitmap_ = nullptr;
  HGDIOBJ old_bitmap_ = nullptr;
  std::uint8_t* bits_ = nullptr;
  int width_ = 0;
  int height_ = 0;
};

std::size_t ForegroundCount(const BinaryGrid& grid) {
  std::size_t count = 0;
  for (std::uint8_t value : grid.pixels) count += value != 0;
  return count;
}

std::vector<BinaryGrid> UsefulComponents(const BinaryGrid& roi) {
  std::vector<BinaryGrid> components = arcassist::SegmentGlyphs(roi);
  int tallest = 0;
  for (const auto& component : components) tallest = std::max(tallest, component.height);

  std::vector<BinaryGrid> kept;
  kept.reserve(components.size());
  for (auto& component : components) {
    const std::size_t ink = ForegroundCount(component);
    if (ink < 1 || component.width <= 0 || component.height <= 0) continue;
    if (component.width > std::max(24, roi.width / 2)) continue;
    if (component.height > roi.height) continue;
    const bool normal_height = component.height >= std::max(2, tallest / 3);
    const bool punctuation = component.height <= std::max(4, tallest / 2) &&
                             component.width <= std::max(8, tallest);
    if (!normal_height && !punctuation) continue;
    kept.push_back(std::move(component));
  }
  return kept;
}

BinaryGrid RenderOneGlyph(HDC dc, HBITMAP bitmap, std::uint8_t* bits,
                          HFONT font, wchar_t glyph) {
  constexpr int width = 64;
  constexpr int height = 64;
  SelectObject(dc, bitmap);
  std::fill(bits, bits + width * height * 4, std::uint8_t{0});
  SetBkColor(dc, RGB(0, 0, 0));
  SetTextColor(dc, RGB(255, 255, 255));
  SetBkMode(dc, OPAQUE);
  HGDIOBJ old_font = SelectObject(dc, font);
  wchar_t text[2] = {glyph, L'\0'};
  TextOutW(dc, 8, 8, text, 1);
  GdiFlush();
  SelectObject(dc, old_font);

  std::vector<std::uint8_t> gray(width * height, 0);
  for (int i = 0; i < width * height; ++i) {
    gray[static_cast<std::size_t>(i)] =
        std::max({bits[4 * i + 0], bits[4 * i + 1], bits[4 * i + 2]});
  }
  BinaryGrid full = arcassist::ThresholdBright(gray.data(), width, height, 96);
  std::vector<BinaryGrid> components = arcassist::SegmentGlyphs(full);
  if (components.empty()) return {};
  auto best = std::max_element(
      components.begin(), components.end(),
      [](const BinaryGrid& a, const BinaryGrid& b) {
        return ForegroundCount(a) < ForegroundCount(b);
      });
  return *best;
}

std::vector<GlyphTemplate> BuildWindowsFontTemplates() {
  std::vector<GlyphTemplate> result;
  const auto fallback = arcassist::BuildGlyphTemplates();
  for (const auto& item : fallback) {
    result.push_back({item.id, arcassist::NormalizeGlyph(item.grid, kNormW, kNormH)});
  }

  HDC screen = GetDC(nullptr);
  if (!screen) return result;
  HDC dc = CreateCompatibleDC(screen);
  if (!dc) {
    ReleaseDC(nullptr, screen);
    return result;
  }

  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = 64;
  info.bmiHeader.biHeight = -64;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  std::uint8_t* bits = nullptr;
  HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS,
                                    reinterpret_cast<void**>(&bits), nullptr, 0);
  if (!bitmap || !bits) {
    if (bitmap) DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
    return result;
  }
  HGDIOBJ old_bitmap = SelectObject(dc, bitmap);

  constexpr wchar_t glyphs[] = L"-+.0123456789";
  constexpr const wchar_t* faces[] = {L"Tahoma", L"Consolas", L"Arial"};
  constexpr int heights[] = {14, 17, 20};
  for (const wchar_t* face : faces) {
    for (int height : heights) {
      HFONT font = CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                               DEFAULT_PITCH, face);
      if (!font) continue;
      for (wchar_t glyph : glyphs) {
        if (glyph == L'\0') break;
        BinaryGrid rendered = RenderOneGlyph(dc, bitmap, bits, font, glyph);
        if (ForegroundCount(rendered) >= 2) {
          result.push_back({static_cast<char>(glyph),
                            arcassist::NormalizeGlyph(rendered, kNormW, kNormH)});
        }
      }
      DeleteObject(font);
    }
  }

  if (old_bitmap) SelectObject(dc, old_bitmap);
  DeleteObject(bitmap);
  DeleteDC(dc);
  ReleaseDC(nullptr, screen);
  return result;
}

struct GlyphGuess {
  char value = '?';
  double score = 0.0;
};

GlyphGuess GuessGlyph(const BinaryGrid& glyph,
                      const std::vector<GlyphTemplate>& templates) {
  GlyphGuess best;
  const BinaryGrid normalized = arcassist::NormalizeGlyph(glyph, kNormW, kNormH);
  for (const auto& item : templates) {
    if (item.grid.width != kNormW || item.grid.height != kNormH) continue;
    const double overlap = arcassist::OverlapScore(normalized, item.grid);
    const double equal = arcassist::MatchScore(normalized, item.grid);
    const double score = 0.78 * overlap + 0.22 * equal;
    if (score > best.score) {
      best.value = item.id;
      best.score = score;
    }
  }
  if (best.score < 0.43) best.value = '?';
  return best;
}

std::optional<double> ParseRawAngle(std::string_view token) {
  if (token.empty()) return std::nullopt;
  std::size_t i = 0;
  if (token[i] == '+' || token[i] == '-') ++i;
  const std::size_t integer_start = i;
  while (i < token.size() && token[i] >= '0' && token[i] <= '9') ++i;
  if (i == integer_start) return std::nullopt;
  if (i < token.size() && token[i] == '.') {
    ++i;
    const std::size_t fraction_start = i;
    while (i < token.size() && token[i] >= '0' && token[i] <= '9') ++i;
    if (i == fraction_start) return std::nullopt;
  }
  if (i != token.size()) return std::nullopt;

  double value = 0.0;
  double scale = 1.0;
  bool fraction = false;
  std::size_t start = (token[0] == '+' || token[0] == '-') ? 1 : 0;
  for (std::size_t n = start; n < token.size(); ++n) {
    if (token[n] == '.') {
      fraction = true;
      continue;
    }
    if (!fraction) value = value * 10.0 + static_cast<double>(token[n] - '0');
    else {
      scale *= 10.0;
      value += static_cast<double>(token[n] - '0') / scale;
    }
  }
  if (token[0] == '-') value = -value;
  if (!std::isfinite(value) || std::fabs(value) > 360.0) return std::nullopt;
  return value;
}

struct OcrResult {
  bool valid = false;
  double raw_angle = 0.0;
  double normalized_pitch = 0.0;
  double confidence = 0.0;
  std::string glyphs;
  std::string token;
};

OcrResult RecognizePitch(const BinaryGrid& roi,
                         const std::vector<GlyphTemplate>& templates) {
  OcrResult result;
  const std::vector<BinaryGrid> components = UsefulComponents(roi);
  if (components.empty() || components.size() > 16) return result;

  double score_sum = 0.0;
  for (const auto& component : components) {
    const GlyphGuess guess = GuessGlyph(component, templates);
    result.glyphs.push_back(guess.value);
    if (guess.value == '?') return result;
    score_sum += guess.score;
  }

  // The ROI contract is deliberately strict: it contains exactly one pitch
  // token. Never accept a numeric substring, because dropping a leading '-'
  // would reverse the Source pitch convention.
  const auto raw = ParseRawAngle(result.glyphs);
  if (!raw) return result;
  const double normalized = arcassist::NormalizePitchDegrees(*raw);
  if (std::fabs(normalized) > arcassist::kMaxPitchDeg) return result;

  result.valid = true;
  result.raw_angle = *raw;
  result.normalized_pitch = normalized;
  result.confidence = score_sum / static_cast<double>(components.size());
  result.token = result.glyphs;
  return result;
}

class LearnedTemplateFile {
 public:
  explicit LearnedTemplateFile(std::wstring path) : path_(std::move(path)) {}

  bool Load(std::vector<GlyphTemplate>* learned) const {
    if (!learned) return false;
    FILE* file = nullptr;
    if (_wfopen_s(&file, path_.c_str(), L"rb") != 0 || !file) return false;
    std::uint32_t header[4]{};
    const bool header_ok = std::fread(header, sizeof(header), 1, file) == 1 &&
                           header[0] == kTemplateMagic &&
                           header[1] == static_cast<std::uint32_t>(kNormW) &&
                           header[2] == static_cast<std::uint32_t>(kNormH) &&
                           header[3] <= 512;
    if (!header_ok) {
      std::fclose(file);
      return false;
    }
    std::vector<GlyphTemplate> loaded;
    loaded.reserve(header[3]);
    for (std::uint32_t i = 0; i < header[3]; ++i) {
      char id = 0;
      BinaryGrid grid;
      grid.width = kNormW;
      grid.height = kNormH;
      grid.pixels.resize(static_cast<std::size_t>(kNormW) * kNormH);
      if (std::fread(&id, 1, 1, file) != 1 ||
          std::fread(grid.pixels.data(), grid.pixels.size(), 1, file) != 1) {
        std::fclose(file);
        return false;
      }
      if (std::string_view("-+.0123456789").find(id) == std::string_view::npos) {
        std::fclose(file);
        return false;
      }
      loaded.push_back({id, std::move(grid)});
    }
    std::fclose(file);
    *learned = std::move(loaded);
    return true;
  }

  bool Save(const std::vector<GlyphTemplate>& learned) const {
    FILE* file = nullptr;
    if (_wfopen_s(&file, path_.c_str(), L"wb") != 0 || !file) return false;
    const std::uint32_t header[4] = {
        kTemplateMagic, static_cast<std::uint32_t>(kNormW),
        static_cast<std::uint32_t>(kNormH),
        static_cast<std::uint32_t>(std::min<std::size_t>(learned.size(), 512))};
    bool ok = std::fwrite(header, sizeof(header), 1, file) == 1;
    for (std::size_t i = 0; ok && i < header[3]; ++i) {
      const GlyphTemplate& item = learned[i];
      ok = item.grid.width == kNormW && item.grid.height == kNormH &&
           item.grid.pixels.size() == static_cast<std::size_t>(kNormW) * kNormH &&
           std::fwrite(&item.id, 1, 1, file) == 1 &&
           std::fwrite(item.grid.pixels.data(), item.grid.pixels.size(), 1, file) == 1;
    }
    std::fclose(file);
    return ok;
  }

 private:
  std::wstring path_;
};

class App {
 public:
  App()
      : ini_path_(ModuleSiblingPath(L"ApexArcAssist.ini")),
        learned_file_(ModuleSiblingPath(L"ApexArcAssist.templates")) {}

  ~App() { ReleaseUiObjects(); }

  bool Initialize(HINSTANCE instance) {
    instance_ = instance;
    LoadConfig();
    ClampRoiToVirtualScreen();
    templates_ = BuildWindowsFontTemplates();
    learned_file_.Load(&learned_templates_);
    templates_.insert(templates_.end(), learned_templates_.begin(),
                      learned_templates_.end());

    // Cache overlay GDI objects once. Recreating fonts and brushes at the
    // 12 Hz near-target rate would cost more than the tiny glyph matcher.
    title_font_ = CreateFontW(-22, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE,
                              FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Microsoft YaHei UI");
    body_font_ = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Microsoft YaHei UI");
    small_font_ = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Microsoft YaHei UI");
    background_brush_ = CreateSolidBrush(RGB(18, 20, 24));
    ready_pen_ = CreatePen(PS_SOLID, 2, RGB(92, 235, 135));
    armed_pen_ = CreatePen(PS_SOLID, 2, RGB(255, 198, 75));
    if (!title_font_ || !body_font_ || !small_font_ || !background_brush_ ||
        !ready_pen_ || !armed_pen_) {
      return false;
    }

    WNDCLASSEXW overlay_class{};
    overlay_class.cbSize = sizeof(overlay_class);
    overlay_class.lpfnWndProc = &App::OverlayProc;
    overlay_class.hInstance = instance_;
    overlay_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    overlay_class.hbrBackground =
        reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    overlay_class.lpszClassName = kOverlayClass;
    if (!RegisterClassExW(&overlay_class)) return false;

    WNDCLASSEXW roi_class{};
    roi_class.cbSize = sizeof(roi_class);
    roi_class.lpfnWndProc = &App::RoiProc;
    roi_class.hInstance = instance_;
    roi_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    roi_class.hbrBackground =
        reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    roi_class.lpszClassName = kRoiClass;
    if (!RegisterClassExW(&roi_class)) return false;

    overlay_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
            WS_EX_LAYERED | WS_EX_TRANSPARENT,
        kOverlayClass, kTitle, WS_POPUP, 0, 0, kOverlayWidth, kOverlayHeight,
        nullptr, nullptr, instance_, this);
    if (!overlay_) return false;
    SetLayeredWindowAttributes(overlay_, 0, 225, LWA_ALPHA);

    roi_window_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
            WS_EX_LAYERED | WS_EX_TRANSPARENT,
        kRoiClass, L"ROI", WS_POPUP, roi_.x, roi_.y, roi_.w, roi_.h,
        nullptr, nullptr, instance_, this);
    if (!roi_window_) return false;
    SetLayeredWindowAttributes(roi_window_, RGB(0, 0, 0), 255,
                               LWA_COLORKEY | LWA_ALPHA);

    PositionOverlay();
    if (!RegisterStaticHotkeys()) {
      status_message_ = L"部分全局热键注册失败；请关闭占用相同按键的程序。";
    }
    ShowWindow(overlay_, SW_SHOWNOACTIVATE);
    UpdateOverlay();
    return true;
  }

  int Run() {
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
  }

  void Shutdown() {
    KillCaptureTimer();
    UnregisterDynamicHotkeys();
    if (overlay_) {
      for (int id : {HK_TRAIN, HK_PAUSE, HK_SIGN, HK_DISTANCE, HK_CALIBRATE,
                     HK_SOUND, HK_QUIT}) {
        UnregisterHotKey(overlay_, id);
      }
    }
    SaveConfig();
    capture_.Reset();
    ReleaseUiObjects();
  }

 private:
  void ReleaseUiObjects() {
    if (title_font_) DeleteObject(title_font_);
    if (body_font_) DeleteObject(body_font_);
    if (small_font_) DeleteObject(small_font_);
    if (background_brush_) DeleteObject(background_brush_);
    if (ready_pen_) DeleteObject(ready_pen_);
    if (armed_pen_) DeleteObject(armed_pen_);
    title_font_ = nullptr;
    body_font_ = nullptr;
    small_font_ = nullptr;
    background_brush_ = nullptr;
    ready_pen_ = nullptr;
    armed_pen_ = nullptr;
  }

  static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT message, WPARAM wparam,
                                      LPARAM lparam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
      app = static_cast<App*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(hwnd, message, wparam, lparam);
    return app->HandleOverlayMessage(hwnd, message, wparam, lparam);
  }

  static LRESULT CALLBACK RoiProc(HWND hwnd, UINT message, WPARAM wparam,
                                  LPARAM lparam) {
    App* app = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
      const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
      app = static_cast<App*>(create->lpCreateParams);
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    if (!app) return DefWindowProcW(hwnd, message, wparam, lparam);
    if (message == WM_PAINT) {
      PAINTSTRUCT paint{};
      HDC dc = BeginPaint(hwnd, &paint);
      RECT rect{};
      GetClientRect(hwnd, &rect);
      FillRect(dc, &rect,
               reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
      HPEN pen = CreatePen(PS_SOLID, 2, RGB(255, 90, 30));
      HGDIOBJ old_pen = SelectObject(dc, pen);
      HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
      Rectangle(dc, 0, 0, rect.right, rect.bottom);
      SelectObject(dc, old_brush);
      SelectObject(dc, old_pen);
      DeleteObject(pen);
      EndPaint(hwnd, &paint);
      return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
  }

  LRESULT HandleOverlayMessage(HWND hwnd, UINT message, WPARAM wparam,
                               LPARAM lparam) {
    switch (message) {
      case WM_PAINT:
        PaintOverlay(hwnd);
        return 0;
      case WM_TIMER:
        if (wparam == kCaptureTimer) CaptureTick();
        return 0;
      case WM_HOTKEY:
        HandleHotkey(static_cast<int>(wparam));
        return 0;
      case WM_DISPLAYCHANGE:
        PositionOverlay();
        ClampRoiToVirtualScreen();
        if (mode_ == UiMode::kCalibrate) MoveRoiWindow();
        capture_.Reset();
        last_hash_.reset();
        last_recognition_ = {};
        status_message_ = L"显示模式已变化；ROI 已限制在虚拟桌面内，建议按 F9 检查。";
        UpdateOverlay();
        return 0;
      case WM_NCHITTEST:
        return HTTRANSPARENT;
      case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
      default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
  }

  void LoadConfig() {
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    roi_.x = ReadIniInt(ini_path_, L"capture", L"roi_x", vx + 48);
    roi_.y = ReadIniInt(ini_path_, L"capture", L"roi_y", vy + 36);
    roi_.w = std::clamp(
        ReadIniInt(ini_path_, L"capture", L"roi_w", arcassist::kDefaultRoiWidth),
        arcassist::kMinRoiWidth, arcassist::kMaxRoiWidth);
    roi_.h = std::clamp(
        ReadIniInt(ini_path_, L"capture", L"roi_h", arcassist::kDefaultRoiHeight),
        arcassist::kMinRoiHeight, arcassist::kMaxRoiHeight);
    threshold_ = std::clamp(ReadIniInt(ini_path_, L"capture", L"threshold", 205),
                            120, 250);
    convention_ = ReadIniInt(ini_path_, L"general", L"up_is_positive", 0) != 0
                      ? PitchConvention::kUpIsPositive
                      : PitchConvention::kUpIsNegative;
    sound_ = ReadIniInt(ini_path_, L"general", L"sound", 1) != 0;
    last_distance_ = std::clamp(
        ReadIniDouble(ini_path_, L"general", L"last_distance", 30.0), 1.0, 60.0);
  }

  void SaveConfig() const {
    WriteIniInt(ini_path_, L"capture", L"roi_x", roi_.x);
    WriteIniInt(ini_path_, L"capture", L"roi_y", roi_.y);
    WriteIniInt(ini_path_, L"capture", L"roi_w", roi_.w);
    WriteIniInt(ini_path_, L"capture", L"roi_h", roi_.h);
    WriteIniInt(ini_path_, L"capture", L"threshold", threshold_);
    WriteIniInt(ini_path_, L"general", L"up_is_positive",
                convention_ == PitchConvention::kUpIsPositive ? 1 : 0);
    WriteIniInt(ini_path_, L"general", L"sound", sound_ ? 1 : 0);
    WriteIniDouble(ini_path_, L"general", L"last_distance", last_distance_);
  }

  bool RegisterOne(int id, UINT modifiers, UINT key,
                   bool suppress_repeat = true) const {
    const UINT effective_modifiers =
        suppress_repeat ? modifiers | MOD_NOREPEAT : modifiers;
    return RegisterHotKey(overlay_, id, effective_modifiers, key) != FALSE;
  }

  bool RegisterStaticHotkeys() {
    bool ok = true;
    ok = RegisterOne(HK_TRAIN, 0, VK_F5) && ok;
    ok = RegisterOne(HK_PAUSE, 0, VK_F6) && ok;
    ok = RegisterOne(HK_SIGN, 0, VK_F7) && ok;
    ok = RegisterOne(HK_DISTANCE, 0, VK_F8) && ok;
    ok = RegisterOne(HK_CALIBRATE, 0, VK_F9) && ok;
    ok = RegisterOne(HK_SOUND, 0, VK_F10) && ok;
    ok = RegisterOne(HK_QUIT, MOD_CONTROL | MOD_SHIFT, 'Q') && ok;
    return ok;
  }

  void UnregisterDynamicHotkeys() {
    if (!overlay_) return;
    for (int id = HK_TEXT_DIGIT_BASE; id <= HK_CAL_COMPACT; ++id) {
      UnregisterHotKey(overlay_, id);
    }
  }

  bool RegisterTextHotkeys(bool allow_sign) {
    UnregisterDynamicHotkeys();
    bool ok = true;
    for (int n = 0; n <= 9; ++n) {
      ok = RegisterOne(HK_TEXT_DIGIT_BASE + n, 0, static_cast<UINT>('0' + n)) && ok;
      ok = RegisterOne(HK_TEXT_NUMPAD_BASE + n, 0,
                       static_cast<UINT>(VK_NUMPAD0 + n)) && ok;
    }
    ok = RegisterOne(HK_TEXT_PERIOD, 0, VK_OEM_PERIOD) && ok;
    ok = RegisterOne(HK_TEXT_DECIMAL, 0, VK_DECIMAL) && ok;
    if (allow_sign) {
      ok = RegisterOne(HK_TEXT_MINUS, 0, VK_OEM_MINUS) && ok;
      ok = RegisterOne(HK_TEXT_PLUS, MOD_SHIFT, VK_OEM_PLUS) && ok;
    }
    ok = RegisterOne(HK_TEXT_BACK, 0, VK_BACK) && ok;
    ok = RegisterOne(HK_TEXT_ENTER, 0, VK_RETURN) && ok;
    ok = RegisterOne(HK_TEXT_ESCAPE, 0, VK_ESCAPE) && ok;
    return ok;
  }

  bool RegisterCalibrationHotkeys() {
    UnregisterDynamicHotkeys();
    bool ok = true;
    // Calibration controls deliberately allow key repeat so the box can be
    // moved or resized quickly while a key is held down.
    ok = RegisterOne(HK_CAL_LEFT, 0, VK_LEFT, false) && ok;
    ok = RegisterOne(HK_CAL_RIGHT, 0, VK_RIGHT, false) && ok;
    ok = RegisterOne(HK_CAL_UP, 0, VK_UP, false) && ok;
    ok = RegisterOne(HK_CAL_DOWN, 0, VK_DOWN, false) && ok;
    ok = RegisterOne(HK_CAL_SHIFT_LEFT, MOD_SHIFT, VK_LEFT, false) && ok;
    ok = RegisterOne(HK_CAL_SHIFT_RIGHT, MOD_SHIFT, VK_RIGHT, false) && ok;
    ok = RegisterOne(HK_CAL_SHIFT_UP, MOD_SHIFT, VK_UP, false) && ok;
    ok = RegisterOne(HK_CAL_SHIFT_DOWN, MOD_SHIFT, VK_DOWN, false) && ok;
    ok = RegisterOne(HK_CAL_CTRL_LEFT, MOD_CONTROL, VK_LEFT, false) && ok;
    ok = RegisterOne(HK_CAL_CTRL_RIGHT, MOD_CONTROL, VK_RIGHT, false) && ok;
    ok = RegisterOne(HK_CAL_CTRL_UP, MOD_CONTROL, VK_UP, false) && ok;
    ok = RegisterOne(HK_CAL_CTRL_DOWN, MOD_CONTROL, VK_DOWN, false) && ok;
    ok = RegisterOne(HK_CAL_THRESHOLD_UP, 0, VK_PRIOR, false) && ok;
    ok = RegisterOne(HK_CAL_THRESHOLD_DOWN, 0, VK_NEXT, false) && ok;
    ok = RegisterOne(HK_CAL_SHRINK, 0, VK_OEM_MINUS, false) && ok;
    ok = RegisterOne(HK_CAL_SHRINK_NUMPAD, 0, VK_SUBTRACT, false) && ok;
    ok = RegisterOne(HK_CAL_GROW, 0, VK_OEM_PLUS, false) && ok;
    ok = RegisterOne(HK_CAL_GROW_SHIFT, MOD_SHIFT, VK_OEM_PLUS, false) && ok;
    ok = RegisterOne(HK_CAL_GROW_NUMPAD, 0, VK_ADD, false) && ok;
    ok = RegisterOne(HK_CAL_COMPACT, 0, VK_HOME) && ok;
    ok = RegisterOne(HK_CAL_ENTER, 0, VK_RETURN) && ok;
    ok = RegisterOne(HK_CAL_ESCAPE, 0, VK_ESCAPE) && ok;
    return ok;
  }

  void HandleHotkey(int id) {
    if (id == HK_QUIT) {
      DestroyWindow(overlay_);
      return;
    }
    if (id == HK_PAUSE) {
      paused_ = !paused_;
      ready_gate_.Reset();
      if (paused_) KillCaptureTimer();
      else ScheduleCapture();
      status_message_ = paused_ ? L"已暂停：零屏幕采集。" : L"已恢复。";
      UpdateOverlay();
      return;
    }
    if (id == HK_SIGN) {
      convention_ = convention_ == PitchConvention::kUpIsNegative
                        ? PitchConvention::kUpIsPositive
                        : PitchConvention::kUpIsNegative;
      ready_gate_.Reset();
      stabilizer_.Reset();
      jump_candidate_.reset();
      current_pitch_.reset();
      current_elevation_.reset();
      SaveConfig();
      status_message_ = convention_ == PitchConvention::kUpIsNegative
                            ? L"pitch 约定：抬头为负。"
                            : L"pitch 约定：抬头为正。";
      UpdateOverlay();
      return;
    }
    if (id == HK_SOUND) {
      sound_ = !sound_;
      SaveConfig();
      status_message_ = sound_ ? L"提示音已开启。" : L"提示音已关闭。";
      UpdateOverlay();
      return;
    }
    if (id == HK_DISTANCE) {
      BeginDistanceEntry();
      return;
    }
    if (id == HK_CALIBRATE) {
      BeginCalibration();
      return;
    }
    if (id == HK_TRAIN) {
      BeginTemplateLearning();
      return;
    }

    if (mode_ == UiMode::kDistance || mode_ == UiMode::kTrainLabel) {
      HandleTextHotkey(id);
      return;
    }
    if (mode_ == UiMode::kCalibrate) HandleCalibrationHotkey(id);
  }

  void BeginDistanceEntry() {
    if (mode_ != UiMode::kNormal) {
      EndTransientMode(false);
      return;
    }
    KillCaptureTimer();
    mode_ = UiMode::kDistance;
    text_input_.clear();
    if (!RegisterTextHotkeys(false)) {
      status_message_ = L"距离输入热键注册不完整。";
    } else {
      status_message_ = L"输入 1–60 米；Enter 确认，空 Enter 复用上次，Esc 取消。";
    }
    UpdateOverlay();
  }

  void BeginCalibration() {
    if (mode_ != UiMode::kNormal) {
      EndTransientMode(false);
      return;
    }
    KillCaptureTimer();
    saved_roi_ = roi_;
    saved_threshold_ = threshold_;
    mode_ = UiMode::kCalibrate;
    const bool hotkeys_ok = RegisterCalibrationHotkeys();
    ClampRoiToVirtualScreen();
    MoveRoiWindow();
    ShowWindow(roi_window_, SW_SHOWNOACTIVATE);
    status_message_ = hotkeys_ok
                          ? L"F9 校准：-/小键盘-缩小，=/+/小键盘+放大，Home恢复72×18。"
                          : L"F9 校准热键注册不完整；请关闭占用方向键或 +/- 的程序后重试。";
    UpdateOverlay();
  }

  void BeginTemplateLearning() {
    if (mode_ != UiMode::kNormal) {
      EndTransientMode(false);
      return;
    }
    KillCaptureTimer();
    BinaryGrid snapshot;
    const std::int64_t start = clock_.NowTicks();
    const bool grabbed = capture_.Grab(roi_, threshold_, &snapshot);
    capture_stats_.Add(clock_.ToMicroseconds(clock_.NowTicks() - start));
    if (!grabbed) {
      status_message_ = L"F5 截取失败；请使用无边框模式并检查 ROI。";
      ScheduleCapture();
      UpdateOverlay();
      return;
    }
    pending_training_components_ = UsefulComponents(snapshot);
    if (pending_training_components_.empty() ||
        pending_training_components_.size() > 8) {
      status_message_ = L"F5 未分割出 1–8 个有效字形；请先按 F9 缩紧 ROI/调阈值。";
      ScheduleCapture();
      UpdateOverlay();
      return;
    }
    mode_ = UiMode::kTrainLabel;
    text_input_.clear();
    RegisterTextHotkeys(true);
    status_message_ = L"已单次截取字形；请原样输入屏幕上的 pitch（含负号和小数点）后 Enter。";
    UpdateOverlay();
  }

  void EndTransientMode(bool save) {
    if (mode_ == UiMode::kCalibrate) {
      ShowWindow(roi_window_, SW_HIDE);
      if (!save) {
        roi_ = saved_roi_;
        threshold_ = saved_threshold_;
      } else {
        SaveConfig();
        capture_.Reset();
        last_hash_.reset();
        last_recognition_ = {};
        stabilizer_.Reset();
        jump_candidate_.reset();
        current_pitch_.reset();
        current_elevation_.reset();
      }
    }
    mode_ = UiMode::kNormal;
    text_input_.clear();
    pending_training_components_.clear();
    UnregisterDynamicHotkeys();
    ScheduleCapture();
    UpdateOverlay();
  }

  void HandleTextHotkey(int id) {
    if ((id >= HK_TEXT_DIGIT_BASE && id < HK_TEXT_DIGIT_BASE + 10) ||
        (id >= HK_TEXT_NUMPAD_BASE && id < HK_TEXT_NUMPAD_BASE + 10)) {
      const int digit = id >= HK_TEXT_NUMPAD_BASE
                            ? id - HK_TEXT_NUMPAD_BASE
                            : id - HK_TEXT_DIGIT_BASE;
      if (text_input_.size() < 8) text_input_.push_back(static_cast<wchar_t>(L'0' + digit));
    } else if (id == HK_TEXT_PERIOD || id == HK_TEXT_DECIMAL) {
      if (text_input_.size() < 8 && text_input_.find(L'.') == std::wstring::npos) {
        text_input_.push_back(L'.');
      }
    } else if (id == HK_TEXT_MINUS && mode_ == UiMode::kTrainLabel) {
      if (text_input_.empty()) text_input_.push_back(L'-');
    } else if (id == HK_TEXT_PLUS && mode_ == UiMode::kTrainLabel) {
      if (text_input_.empty()) text_input_.push_back(L'+');
    } else if (id == HK_TEXT_BACK) {
      if (!text_input_.empty()) text_input_.pop_back();
    } else if (id == HK_TEXT_ESCAPE) {
      status_message_ = L"已取消。";
      EndTransientMode(false);
      return;
    } else if (id == HK_TEXT_ENTER) {
      if (mode_ == UiMode::kDistance) CommitDistance();
      else CommitTemplateLearning();
      return;
    }
    UpdateOverlay();
  }

  void CommitDistance() {
    double distance = last_distance_;
    if (!text_input_.empty()) {
      wchar_t* end = nullptr;
      distance = std::wcstod(text_input_.c_str(), &end);
      if (end == text_input_.c_str() || *end != L'\0' ||
          !std::isfinite(distance) || distance < 1.0 || distance > 60.0) {
        status_message_ = L"距离无效：请输入 1–60 米。";
        UpdateOverlay();
        return;
      }
    }
    const auto angle = arcassist::HighArcElevationDegrees(distance);
    if (!angle) {
      status_message_ = L"距离超出公式定义域。";
      UpdateOverlay();
      return;
    }
    last_distance_ = distance;
    distance_m_ = distance;
    target_elevation_ = *angle;
    tolerance_ = arcassist::ElevationToleranceDegrees(distance);
    armed_ = true;
    paused_ = false;
    current_pitch_.reset();
    current_elevation_.reset();
    stabilizer_.Reset();
    jump_candidate_.reset();
    last_hash_.reset();
    lost_streak_ = 0;
    ready_gate_.Reset();
    SaveConfig();
    status_message_ = L"目标已装定；仅在此状态采集小 ROI。F6 可立即暂停。";
    EndTransientMode(true);
  }

  void CommitTemplateLearning() {
    std::string label;
    label.reserve(text_input_.size());
    for (wchar_t ch : text_input_) {
      if (ch > 127) {
        status_message_ = L"训练标签只能使用 - + . 和数字。";
        UpdateOverlay();
        return;
      }
      label.push_back(static_cast<char>(ch));
    }
    const auto raw = ParseRawAngle(label);
    if (!raw || label.size() != pending_training_components_.size()) {
      std::wostringstream message;
      message << L"标签/字形不匹配：检测到 " << pending_training_components_.size()
              << L" 个字形，输入了 " << label.size() << L" 个。请检查 ROI。";
      status_message_ = message.str();
      UpdateOverlay();
      return;
    }
    for (std::size_t i = 0; i < label.size(); ++i) {
      if (std::string_view("-+.0123456789").find(label[i]) ==
          std::string_view::npos) {
        status_message_ = L"训练标签含不支持字符。";
        UpdateOverlay();
        return;
      }
      GlyphTemplate item{label[i], arcassist::NormalizeGlyph(
                                      pending_training_components_[i], kNormW, kNormH)};
      learned_templates_.push_back(item);
      templates_.push_back(std::move(item));
    }
    // Force the next sample through the classifier so newly learned templates
    // take effect even when the captured pixels have not changed.
    last_hash_.reset();
    last_recognition_ = {};
    if (!learned_file_.Save(learned_templates_)) {
      status_message_ = L"模板已加入本次运行，但保存失败（检查目录写权限）。";
    } else {
      status_message_ = L"精确字形模板已保存；可在不同角度重复 F5 补齐数字。";
    }
    EndTransientMode(true);
  }

  void HandleCalibrationHotkey(int id) {
    int move_x = 0;
    int move_y = 0;
    if (id == HK_CAL_LEFT) move_x = -1;
    if (id == HK_CAL_RIGHT) move_x = 1;
    if (id == HK_CAL_UP) move_y = -1;
    if (id == HK_CAL_DOWN) move_y = 1;
    if (id == HK_CAL_SHIFT_LEFT) move_x = -10;
    if (id == HK_CAL_SHIFT_RIGHT) move_x = 10;
    if (id == HK_CAL_SHIFT_UP) move_y = -10;
    if (id == HK_CAL_SHIFT_DOWN) move_y = 10;
    roi_.x += move_x;
    roi_.y += move_y;

    if (id == HK_CAL_CTRL_LEFT) {
      roi_ = arcassist::ResizeRoiCentered(roi_, -1, 0);
    }
    if (id == HK_CAL_CTRL_RIGHT) {
      roi_ = arcassist::ResizeRoiCentered(roi_, 1, 0);
    }
    if (id == HK_CAL_CTRL_UP) {
      roi_ = arcassist::ResizeRoiCentered(roi_, 0, -1);
    }
    if (id == HK_CAL_CTRL_DOWN) {
      roi_ = arcassist::ResizeRoiCentered(roi_, 0, 1);
    }
    if (id == HK_CAL_SHRINK || id == HK_CAL_SHRINK_NUMPAD) {
      roi_ = arcassist::ResizeRoiCentered(
          roi_, -arcassist::kQuickResizeWidthStep,
          -arcassist::kQuickResizeHeightStep);
    }
    if (id == HK_CAL_GROW || id == HK_CAL_GROW_SHIFT ||
        id == HK_CAL_GROW_NUMPAD) {
      roi_ = arcassist::ResizeRoiCentered(
          roi_, arcassist::kQuickResizeWidthStep,
          arcassist::kQuickResizeHeightStep);
    }
    if (id == HK_CAL_COMPACT) {
      roi_ = arcassist::CompactRoiCentered(roi_);
    }
    ClampRoiToVirtualScreen();
    if (id == HK_CAL_THRESHOLD_UP) threshold_ = std::min(250, threshold_ + 5);
    if (id == HK_CAL_THRESHOLD_DOWN) threshold_ = std::max(120, threshold_ - 5);
    if (id == HK_CAL_ENTER) {
      status_message_ = L"ROI/阈值已保存；建议立刻按 F5 学习当前字形。";
      EndTransientMode(true);
      return;
    }
    if (id == HK_CAL_ESCAPE) {
      status_message_ = L"校准已取消。";
      EndTransientMode(false);
      return;
    }
    MoveRoiWindow();
    UpdateOverlay();
  }

  void ClampRoiToVirtualScreen() {
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int right = left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int bottom = top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    roi_ = arcassist::ClampRoiToBounds(roi_, left, top, right, bottom);
  }

  bool CanCapture() const {
    return overlay_ && mode_ == UiMode::kNormal && armed_ && !paused_;
  }

  void KillCaptureTimer() {
    if (overlay_) KillTimer(overlay_, kCaptureTimer);
    capture_interval_ms_ = 0;
  }

  UINT DesiredInterval() const {
    if (ready_gate_.ready()) return kReadyIntervalMs;
    if (lost_streak_ >= 8) return kLost3IntervalMs;
    if (lost_streak_ >= 5) return kLost2IntervalMs;
    if (lost_streak_ >= 2) return kLost1IntervalMs;
    if (current_elevation_ && target_elevation_ &&
        std::fabs(*current_elevation_ - *target_elevation_) <= kNearBandDeg) {
      return kNearIntervalMs;
    }
    return kFarIntervalMs;
  }

  void ScheduleCapture() {
    KillCaptureTimer();
    if (!CanCapture()) return;
    capture_interval_ms_ = DesiredInterval();
    SetTimer(overlay_, kCaptureTimer, capture_interval_ms_, nullptr);
  }

  void CaptureTick() {
    if (!CanCapture()) {
      KillCaptureTimer();
      return;
    }
    BinaryGrid binary;
    const std::int64_t capture_start = clock_.NowTicks();
    const bool grabbed = capture_.Grab(roi_, threshold_, &binary);
    capture_stats_.Add(clock_.ToMicroseconds(clock_.NowTicks() - capture_start));
    const double now_ms = clock_.ToMilliseconds(clock_.NowTicks());

    if (!grabbed) {
      OnInvalidSample(now_ms, L"GDI 截取失败；请检查无边框模式。", true);
      return;
    }

    const std::uint64_t hash = arcassist::HashGrid(binary);
    OcrResult result;
    const std::int64_t ocr_start = clock_.NowTicks();
    if (last_hash_ && *last_hash_ == hash && last_recognition_.valid) {
      result = last_recognition_;
      hash_reuses_++;
    } else {
      result = RecognizePitch(binary, templates_);
      last_recognition_ = result;
      last_hash_ = hash;
    }
    ocr_stats_.Add(clock_.ToMicroseconds(clock_.NowTicks() - ocr_start));

    if (!result.valid) {
      OnInvalidSample(now_ms, L"OCR LOST：按 F9 缩紧 ROI，再用 F5 学习字形。", false);
      return;
    }

    const auto elevation = arcassist::PitchToUpwardElevation(
        result.normalized_pitch, convention_);
    if (!elevation || !target_elevation_) {
      OnInvalidSample(now_ms, L"OCR 数值不在有效 pitch 范围。", false);
      return;
    }

    // Reject one-frame OCR spikes. A real fast view movement is admitted after
    // a second nearby sample, so the stabilizer cannot get stuck behind an old
    // angle after a deliberate flick.
    // A valid glyph reading exits OCR-lost backoff immediately, including
    // while a real fast view movement is waiting for its second confirmation.
    lost_streak_ = 0;
    std::optional<double> stable = stabilizer_.Push(now_ms, *elevation);
    if (!stable) {
      const bool confirms_jump =
          jump_candidate_ &&
          std::fabs(*jump_candidate_ - *elevation) <= 1.5 &&
          now_ms - jump_candidate_ms_ <= 600.0;
      if (confirms_jump) {
        stabilizer_.Reset();
        stable = stabilizer_.Push(now_ms, *elevation);
        jump_candidate_.reset();
      } else {
        jump_candidate_ = *elevation;
        jump_candidate_ms_ = now_ms;
        ready_gate_.OnInvalidSample(now_ms);
        status_message_ = L"已拒绝单帧角度跳变；等待下一帧确认。";
        ocr_confidence_ = result.confidence;
        ocr_text_ = result.token;
        UpdateOverlay();
        ScheduleCapture();
        return;
      }
    } else {
      jump_candidate_.reset();
    }

    last_valid_ms_ = now_ms;
    current_elevation_ = *stable;
    current_pitch_ = convention_ == PitchConvention::kUpIsNegative
                         ? -*stable
                         : *stable;
    ocr_confidence_ = result.confidence;
    ocr_text_ = result.token;

    const ReadyEvent event = ready_gate_.Update(
        now_ms, *current_elevation_, *target_elevation_, tolerance_);
    if (event == ReadyEvent::kEntered) {
      status_message_ = L"READY：由你手动投掷。";
      if (sound_) MessageBeep(MB_OK);
    } else if (event == ReadyEvent::kExited) {
      status_message_ = L"已离开目标角度。";
    } else {
      const double error = *current_elevation_ - *target_elevation_;
      if (std::fabs(error) > tolerance_) {
        status_message_ = error < 0.0 ? L"继续抬高。" : L"稍微压低。";
      }
    }
    RefreshPerfSnapshot(now_ms);
    UpdateOverlay();
    ScheduleCapture();
  }

  void OnInvalidSample(double now_ms, const wchar_t* message,
                       bool reset_surface) {
    ++lost_streak_;
    status_message_ = message;
    if (reset_surface) capture_.Reset();
    // READY is a positive claim, so one invalid sample withdraws it
    // immediately. Keep the last numeric display briefly to avoid visual
    // flicker, then clear it after three misses or the stale deadline.
    if (ready_gate_.ready()) ready_gate_.OnInvalidSample(now_ms);
    if (lost_streak_ >= 3 || now_ms - last_valid_ms_ > arcassist::kStaleSampleMs) {
      ready_gate_.OnInvalidSample(now_ms);
      current_pitch_.reset();
      current_elevation_.reset();
      stabilizer_.Reset();
      jump_candidate_.reset();
    }
    RefreshPerfSnapshot(now_ms);
    UpdateOverlay();
    ScheduleCapture();
  }

  void RefreshPerfSnapshot(double now_ms) {
    if (now_ms - last_perf_refresh_ms_ < 1000.0) return;
    capture_perf_ = capture_stats_.MedianAndP95();
    ocr_perf_ = ocr_stats_.MedianAndP95();
    last_perf_refresh_ms_ = now_ms;
  }

  double TargetPitch() const {
    if (!target_elevation_) return 0.0;
    return convention_ == PitchConvention::kUpIsNegative ? -*target_elevation_
                                                          : *target_elevation_;
  }

  void PositionOverlay() {
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    const int x = std::max(work.left, work.right - kOverlayWidth - 18);
    const int y = work.top + 18;
    SetWindowPos(overlay_, HWND_TOPMOST, x, y, kOverlayWidth, kOverlayHeight,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }

  void MoveRoiWindow() {
    SetWindowPos(roi_window_, HWND_TOPMOST, roi_.x, roi_.y, roi_.w, roi_.h,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(roi_window_, nullptr, TRUE);
  }

  void UpdateOverlay() {
    if (!overlay_) return;
    InvalidateRect(overlay_, nullptr, FALSE);
    UpdateWindow(overlay_);
  }

  void DrawLine(HDC dc, int x, int y, const std::wstring& text, COLORREF colour,
                HFONT font) const {
    SetTextColor(dc, colour);
    HGDIOBJ old = SelectObject(dc, font);
    TextOutW(dc, x, y, text.c_str(), static_cast<int>(text.size()));
    SelectObject(dc, old);
  }

  void PaintOverlay(HWND hwnd) {
    PAINTSTRUCT paint{};
    HDC dc = BeginPaint(hwnd, &paint);
    RECT rect{};
    GetClientRect(hwnd, &rect);
    FillRect(dc, &rect, background_brush_);
    SetBkMode(dc, TRANSPARENT);

    HFONT title_font = title_font_;
    HFONT body_font = body_font_;
    HFONT small_font = small_font_;

    const COLORREF green = RGB(92, 235, 135);
    const COLORREF amber = RGB(255, 198, 75);
    const COLORREF white = RGB(236, 239, 244);
    const COLORREF muted = RGB(170, 179, 190);
    const COLORREF state_colour = ready_gate_.ready() ? green : amber;

    std::wstring headline = L"Apex Arc Assist  |  PIXEL-ONLY  |  ";
    if (paused_) headline += L"PAUSED / 0 CAPTURE";
    else if (!armed_) headline += L"IDLE / 0 CAPTURE";
    else headline += ready_gate_.ready() ? L"READY" : L"ARMED";
    DrawLine(dc, 16, 12, headline, state_colour, title_font);

    int y = 48;
    if (mode_ == UiMode::kDistance) {
      DrawLine(dc, 16, y, L"距离输入：" + (text_input_.empty() ? L"_" : text_input_) +
                                L" m",
               white, body_font);
      y += 28;
    } else if (mode_ == UiMode::kTrainLabel) {
      std::wostringstream train;
      train << L"F5 字形训练：检测 " << pending_training_components_.size()
            << L" 个，标签 " << (text_input_.empty() ? L"_" : text_input_);
      DrawLine(dc, 16, y, train.str(), white, body_font);
      y += 28;
    } else if (mode_ == UiMode::kCalibrate) {
      std::wostringstream cal;
      cal << L"ROI " << roi_.x << L"," << roi_.y << L"  " << roi_.w << L"×"
          << roi_.h << L"  threshold=" << threshold_
          << L"  [-]缩小 [=]放大 [Home]72×18";
      DrawLine(dc, 16, y, cal.str(), white, body_font);
      y += 28;
    } else if (armed_ && target_elevation_) {
      std::wostringstream target;
      target << L"距离 " << std::fixed << std::setprecision(1) << distance_m_
             << L"m  目标高抛 " << std::setprecision(2) << *target_elevation_
             << L"°  目标 pitch " << TargetPitch() << L"°";
      DrawLine(dc, 16, y, target.str(), white, body_font);
      y += 27;

      if (current_pitch_ && current_elevation_) {
        const double error = *current_elevation_ - *target_elevation_;
        std::wostringstream current;
        current << L"当前 pitch " << std::fixed << std::setprecision(2)
                << *current_pitch_ << L"°  仰角 " << *current_elevation_
                << L"°  误差 " << std::showpos << error << std::noshowpos
                << L"°  ±" << tolerance_ << L"°";
        DrawLine(dc, 16, y, current.str(), ready_gate_.ready() ? green : white,
                 body_font);
      } else {
        DrawLine(dc, 16, y, L"当前 pitch --  |  等待有效字形", amber, body_font);
      }
      y += 27;
    } else {
      DrawLine(dc, 16, y, L"按 F9 校准 ROI，F5 学习字形，F8 输入距离。", white,
               body_font);
      y += 28;
    }

    DrawLine(dc, 16, y, status_message_, state_colour, body_font);
    y += 27;

    std::wostringstream ocr;
    ocr << L"OCR='";
    for (char ch : ocr_text_) ocr << static_cast<wchar_t>(ch);
    ocr << L"' conf=" << std::fixed << std::setprecision(2) << ocr_confidence_
        << L"  interval=" << capture_interval_ms_ << L"ms  hash-reuse="
        << hash_reuses_;
    DrawLine(dc, 16, y, ocr.str(), muted, small_font);
    y += 21;

    std::wostringstream perf;
    perf << L"CAP median/p95 " << std::fixed << std::setprecision(0)
         << capture_perf_.first << L"/" << capture_perf_.second
         << L"us  MATCH " << ocr_perf_.first << L"/" << ocr_perf_.second
         << L"us  learned=" << learned_templates_.size();
    DrawLine(dc, 16, y, perf.str(), muted, small_font);
    y += 20;

    DrawLine(dc, 16, y,
             L"F5学习  F6零采集暂停  F7符号  F8距离  F9 ROI  F10声音  Ctrl+Shift+Q退出",
             muted, small_font);

    HPEN border = ready_gate_.ready() ? ready_pen_ : armed_pen_;
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, 1, 1, rect.right - 1, rect.bottom - 1);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    EndPaint(hwnd, &paint);
  }

  HINSTANCE instance_ = nullptr;
  HWND overlay_ = nullptr;
  HWND roi_window_ = nullptr;
  HFONT title_font_ = nullptr;
  HFONT body_font_ = nullptr;
  HFONT small_font_ = nullptr;
  HBRUSH background_brush_ = nullptr;
  HPEN ready_pen_ = nullptr;
  HPEN armed_pen_ = nullptr;
  std::wstring ini_path_;
  LearnedTemplateFile learned_file_;
  Roi roi_{};
  Roi saved_roi_{};
  int threshold_ = 205;
  int saved_threshold_ = 205;
  UiMode mode_ = UiMode::kNormal;
  std::wstring text_input_;
  std::wstring status_message_ = L"IDLE：当前无屏幕采集。";
  bool armed_ = false;
  bool paused_ = false;
  bool sound_ = true;
  PitchConvention convention_ = PitchConvention::kUpIsNegative;
  double last_distance_ = 30.0;
  double distance_m_ = 0.0;
  std::optional<double> target_elevation_;
  double tolerance_ = arcassist::kToleranceFloorDeg;
  std::optional<double> current_pitch_;
  std::optional<double> current_elevation_;
  arcassist::ElevationStabilizer stabilizer_;
  std::optional<double> jump_candidate_;
  double jump_candidate_ms_ = -100000.0;
  double last_valid_ms_ = -100000.0;
  double ocr_confidence_ = 0.0;
  std::string ocr_text_;
  int lost_streak_ = 0;
  UINT capture_interval_ms_ = 0;
  std::uint64_t hash_reuses_ = 0;
  CaptureSurface capture_;
  std::vector<GlyphTemplate> templates_;
  std::vector<GlyphTemplate> learned_templates_;
  std::vector<BinaryGrid> pending_training_components_;
  std::optional<std::uint64_t> last_hash_;
  OcrResult last_recognition_;
  arcassist::ReadyGate ready_gate_;
  QpcClock clock_;
  RollingStats capture_stats_;
  RollingStats ocr_stats_;
  std::pair<double, double> capture_perf_{0.0, 0.0};
  std::pair<double, double> ocr_perf_{0.0, 0.0};
  double last_perf_refresh_ms_ = -100000.0;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  // The manifest requests PerMonitorV2. This call is a compatibility fallback
  // for older systems and does not create a polling/background path.
  SetProcessDPIAware();
  App app;
  if (!app.Initialize(instance)) {
    MessageBoxW(nullptr, L"Apex Arc Assist 初始化失败。", kTitle,
                MB_OK | MB_ICONERROR);
    return 1;
  }
  const int result = app.Run();
  app.Shutdown();
  return result;
}
