// Apex Arc Assist — angle-ruler edition.
// Native Win32, pixel-free angle calculator. No screen capture, no OCR,
// no game access. The user reads the ruler and throws manually.
//
// F8: enter distance (1-60m). F9: toggle the semi-transparent ruler panel.
// Ctrl+Shift+Q: exit. The panel is click-through and never steals focus.

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

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using arcassist::HighArcElevationDegrees;

constexpr wchar_t kOverlayClass[] = L"ApexArcAssistOverlay";
constexpr wchar_t kTitle[] = L"Apex Arc Assist";
// Panel geometry (physical pixels). Position: top-left, below the in-game
// cl_showpos HUD block. Measured from the user's 2560x1440 pic.png: the HUD
// text occupies y≈36..76, so the panel starts at y=96 with a 16px gap.
constexpr int kOverlayWidth = 330;
constexpr int kOverlayHeight = 352;
constexpr int kOverlayMarginX = 12;
constexpr int kOverlayMarginY = 96;
constexpr int kRowHeight = 38;
constexpr int kTableTop = 88;

constexpr int HK_DISTANCE = 1;
constexpr int HK_RULER = 2;
constexpr int HK_QUIT = 3;
constexpr int HK_TEXT_DIGIT_BASE = 100;
constexpr int HK_TEXT_NUMPAD_BASE = 110;
constexpr int HK_TEXT_PERIOD = 120;
constexpr int HK_TEXT_DECIMAL = 121;
constexpr int HK_TEXT_BACK = 124;
constexpr int HK_TEXT_ENTER = 125;
constexpr int HK_TEXT_ESCAPE = 126;

// Ruler rows: 10, 20, 30, 40, 50, 60 metres.
struct RulerRow {
  double distance;
  double angle;
};

constexpr std::array<RulerRow, 6> kRuler = {{
    {10.0, 0.0}, {20.0, 0.0}, {30.0, 0.0},
    {40.0, 0.0}, {50.0, 0.0}, {60.0, 0.0},
}};

double RulerAngle(double distance) {
  const auto angle = HighArcElevationDegrees(distance);
  return angle ? *angle : 0.0;
}

std::wstring WidenAscii(std::string_view text) {
  std::wstring result;
  result.reserve(text.size());
  for (char ch : text) {
    result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
  }
  return result;
}

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

int ReadIniInt(const std::wstring& path, const wchar_t* section,
               const wchar_t* key, int fallback) {
  wchar_t buffer[32]{};
  wchar_t fallback_text[32]{};
  _snwprintf_s(fallback_text, _TRUNCATE, L"%d", fallback);
  GetPrivateProfileStringW(section, key, fallback_text, buffer,
                           static_cast<DWORD>(std::size(buffer)), path.c_str());
  const int value = _wtoi(buffer);
  return value != 0 || buffer[0] == L'0' ? value : fallback;
}

bool WriteIniDouble(const std::wstring& path, const wchar_t* section,
                    const wchar_t* key, double value) {
  wchar_t buffer[64]{};
  _snwprintf_s(buffer, _TRUNCATE, L"%.1f", value);
  return WritePrivateProfileStringW(section, key, buffer, path.c_str()) != FALSE;
}

double ReadIniDouble(const std::wstring& path, const wchar_t* section,
                     const wchar_t* key, double fallback) {
  wchar_t buffer[64]{};
  wchar_t fallback_text[64]{};
  _snwprintf_s(fallback_text, _TRUNCATE, L"%.1f", fallback);
  GetPrivateProfileStringW(section, key, fallback_text, buffer,
                           static_cast<DWORD>(std::size(buffer)), path.c_str());
  return _wtof(buffer);
}

std::optional<double> ParseInvariantDecimal(std::wstring_view text) {
  std::size_t begin = 0;
  std::size_t end = text.size();
  while (begin < end && (text[begin] == L' ' || text[begin] == L'\t')) ++begin;
  while (end > begin && (text[end - 1] == L' ' || text[end - 1] == L'\t')) --end;
  if (begin == end) return std::nullopt;

  double value = 0.0;
  bool has_digits = false;
  bool fraction = false;
  double divisor = 1.0;
  for (std::size_t i = begin; i < end; ++i) {
    const wchar_t ch = text[i];
    if (ch == L'.' && !fraction) {
      fraction = true;
      continue;
    }
    if (ch < L'0' || ch > L'9') return std::nullopt;
    has_digits = true;
    if (!fraction) value = value * 10.0 + static_cast<double>(ch - L'0');
    else {
      divisor *= 10.0;
      value += static_cast<double>(ch - L'0') / divisor;
    }
  }
  if (!has_digits || !std::isfinite(value)) return std::nullopt;
  return value;
}

class App {
 public:
  App() : ini_path_(ModuleSiblingPath(L"ApexArcAssist.ini")) {}
  ~App() { Shutdown(); }

  bool Initialize(HINSTANCE instance) {
    instance_ = instance;
    LoadConfig();
    for (auto& row : ruler_rows_) row.angle = RulerAngle(row.distance);

    title_font_ = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Microsoft YaHei UI");
    body_font_ = CreateFontW(-17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Microsoft YaHei UI");
    ruler_font_ = CreateFontW(-24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Consolas");
    small_font_ = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                              DEFAULT_PITCH, L"Microsoft YaHei UI");
    background_brush_ = CreateSolidBrush(RGB(12, 14, 19));
    highlight_brush_ = CreateSolidBrush(RGB(58, 44, 12));
    if (!title_font_ || !body_font_ || !ruler_font_ || !small_font_ ||
        !background_brush_ || !highlight_brush_) {
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

    overlay_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE |
            WS_EX_LAYERED | WS_EX_TRANSPARENT,
        kOverlayClass, kTitle, WS_POPUP, 0, 0, kOverlayWidth, kOverlayHeight,
        nullptr, nullptr, instance_, this);
    if (!overlay_) return false;
    // Semi-transparent so game data behind the panel stays readable.
    SetLayeredWindowAttributes(overlay_, 0, 185, LWA_ALPHA);

    PositionOverlay();
    if (!RegisterStaticHotkeys()) {
      status_message_ = L"部分热键注册失败；请关闭占用按键的程序。";
    }
    // The ruler starts hidden; F9 toggles it.
    ShowWindow(overlay_, SW_HIDE);
    initialized_ = true;
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
    if (shutdown_) return;
    shutdown_ = true;
    if (overlay_) {
      for (int id : {HK_DISTANCE, HK_RULER, HK_QUIT}) {
        UnregisterHotKey(overlay_, id);
      }
      for (int id = HK_TEXT_DIGIT_BASE; id <= HK_TEXT_ESCAPE; ++id) {
        UnregisterHotKey(overlay_, id);
      }
    }
    if (initialized_) SaveConfig();
    if (overlay_) {
      if (IsWindow(overlay_)) DestroyWindow(overlay_);
      overlay_ = nullptr;
    }
    ReleaseUiObjects();
  }

 private:
  void ReleaseUiObjects() {
    if (title_font_) DeleteObject(title_font_);
    if (body_font_) DeleteObject(body_font_);
    if (ruler_font_) DeleteObject(ruler_font_);
    if (small_font_) DeleteObject(small_font_);
    if (background_brush_) DeleteObject(background_brush_);
    if (highlight_brush_) DeleteObject(highlight_brush_);
    title_font_ = nullptr;
    body_font_ = nullptr;
    ruler_font_ = nullptr;
    small_font_ = nullptr;
    background_brush_ = nullptr;
    highlight_brush_ = nullptr;
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

  LRESULT HandleOverlayMessage(HWND hwnd, UINT message, WPARAM wparam,
                               LPARAM lparam) {
    switch (message) {
      case WM_PAINT:
        PaintOverlay(hwnd);
        return 0;
      case WM_HOTKEY:
        HandleHotkey(static_cast<int>(wparam));
        return 0;
      case WM_DISPLAYCHANGE:
        PositionOverlay();
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
    last_distance_ = std::clamp(
        ReadIniDouble(ini_path_, L"general", L"last_distance", 0.0),
        0.0, arcassist::kMaxDistanceMeters);
  }

  void SaveConfig() const {
    WriteIniDouble(ini_path_, L"general", L"last_distance", last_distance_);
  }

  bool RegisterOne(int id, UINT modifiers, UINT key) const {
    return RegisterHotKey(overlay_, id, modifiers | MOD_NOREPEAT, key) != FALSE;
  }

  bool RegisterStaticHotkeys() {
    bool ok = true;
    ok = RegisterOne(HK_DISTANCE, 0, VK_F8) && ok;
    ok = RegisterOne(HK_RULER, 0, VK_F9) && ok;
    ok = RegisterOne(HK_QUIT, MOD_CONTROL | MOD_SHIFT, 'Q') && ok;
    return ok;
  }

  bool RegisterTextHotkeys() {
    UnregisterTextHotkeys();
    bool ok = true;
    for (int n = 0; n <= 9; ++n) {
      ok = RegisterOne(HK_TEXT_DIGIT_BASE + n, 0,
                       static_cast<UINT>('0' + n)) && ok;
      ok = RegisterOne(HK_TEXT_NUMPAD_BASE + n, 0,
                       static_cast<UINT>(VK_NUMPAD0 + n)) && ok;
    }
    ok = RegisterOne(HK_TEXT_PERIOD, 0, VK_OEM_PERIOD) && ok;
    ok = RegisterOne(HK_TEXT_DECIMAL, 0, VK_DECIMAL) && ok;
    ok = RegisterOne(HK_TEXT_BACK, 0, VK_BACK) && ok;
    ok = RegisterOne(HK_TEXT_ENTER, 0, VK_RETURN) && ok;
    ok = RegisterOne(HK_TEXT_ESCAPE, 0, VK_ESCAPE) && ok;
    return ok;
  }

  void UnregisterTextHotkeys() {
    if (!overlay_) return;
    for (int id = HK_TEXT_DIGIT_BASE; id <= HK_TEXT_ESCAPE; ++id) {
      UnregisterHotKey(overlay_, id);
    }
  }

  void HandleHotkey(int id) {
    if (id == HK_QUIT) {
      DestroyWindow(overlay_);
      return;
    }
    if (id == HK_RULER) {
      ruler_visible_ = !ruler_visible_;
      ShowWindow(overlay_, ruler_visible_ ? SW_SHOWNOACTIVATE : SW_HIDE);
      if (ruler_visible_) UpdateOverlay();
      return;
    }
    if (id == HK_DISTANCE) {
      BeginDistanceEntry();
      return;
    }
    if (mode_ == kInput) HandleTextHotkey(id);
  }

  void BeginDistanceEntry() {
    if (mode_ == kInput) return;
    mode_ = kInput;
    text_input_.clear();
    RegisterTextHotkeys();
    // Make the panel visible so the input is readable.
    ruler_visible_ = true;
    ShowWindow(overlay_, SW_SHOWNOACTIVATE);
    UpdateOverlay();
  }

  void HandleTextHotkey(int id) {
    if ((id >= HK_TEXT_DIGIT_BASE && id < HK_TEXT_DIGIT_BASE + 10) ||
        (id >= HK_TEXT_NUMPAD_BASE && id < HK_TEXT_NUMPAD_BASE + 10)) {
      const int digit = id >= HK_TEXT_NUMPAD_BASE
                            ? id - HK_TEXT_NUMPAD_BASE
                            : id - HK_TEXT_DIGIT_BASE;
      if (text_input_.size() < 6) {
        text_input_.push_back(static_cast<wchar_t>(L'0' + digit));
      }
    } else if (id == HK_TEXT_PERIOD || id == HK_TEXT_DECIMAL) {
      if (text_input_.size() < 6 &&
          text_input_.find(L'.') == std::wstring::npos) {
        text_input_.push_back(L'.');
      }
    } else if (id == HK_TEXT_BACK) {
      if (!text_input_.empty()) text_input_.pop_back();
    } else if (id == HK_TEXT_ESCAPE) {
      EndInput();
      return;
    } else if (id == HK_TEXT_ENTER) {
      CommitDistance();
      return;
    }
    UpdateOverlay();
  }

  void CommitDistance() {
    double distance = last_distance_;
    if (!text_input_.empty()) {
      const auto parsed = ParseInvariantDecimal(text_input_);
      if (!parsed || *parsed < 1.0 || *parsed > 60.0) {
        status_message_ = L"距离无效：请输入 1–60 米。";
        UpdateOverlay();
        return;
      }
      distance = *parsed;
    } else if (last_distance_ <= 0.0) {
      status_message_ = L"还没有距离：请先输入 1–60 米。";
      UpdateOverlay();
      return;
    }
    last_distance_ = distance;
    SaveConfig();
    status_message_.clear();
    EndInput();
  }

  void EndInput() {
    UnregisterTextHotkeys();
    mode_ = kNormal;
    text_input_.clear();
    UpdateOverlay();
  }

  void PositionOverlay() {
    if (!overlay_) return;
    RECT work{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    // Top-left, below the game HUD number block (see geometry comment above).
    const int x = work.left + kOverlayMarginX;
    const int y = work.top + kOverlayMarginY;
    SetWindowPos(overlay_, HWND_TOPMOST, x, y, kOverlayWidth, kOverlayHeight,
                 SWP_NOACTIVATE | (ruler_visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
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

    const COLORREF green = RGB(112, 235, 130);
    const COLORREF amber = RGB(255, 200, 90);
    const COLORREF white = RGB(240, 243, 248);
    const COLORREF muted = RGB(160, 170, 185);

    if (mode_ == kInput) {
      DrawLine(dc, 12, 12, L"输入距离（1–60m）", amber, title_font_);
      std::wstring text = L"距离：" + (text_input_.empty() ? L"_" : text_input_) +
                          L" m   [Enter]确认 [Esc]取消";
      DrawLine(dc, 12, 50, text, white, body_font_);
      EndPaint(hwnd, &paint);
      return;
    }

    DrawLine(dc, 12, 12, L"高抛角度标尺", white, title_font_);
    DrawLine(dc, 232, 18, L"F9 隐藏", muted, small_font_);

    int y = 50;
    if (last_distance_ > 0.0) {
      std::wostringstream current;
      current << L"当前目标  " << std::fixed << std::setprecision(1)
              << last_distance_ << L"m → " << std::setprecision(1)
              << RulerAngle(last_distance_) << L"°";
      DrawLine(dc, 12, y, current.str(), green, body_font_);
    } else {
      DrawLine(dc, 12, y, L"按 F8 输入当前距离", amber, body_font_);
    }

    // Ruler rows: distance on the left, big angle on the right. One row per
    // 38px keeps the 24px bold angle glyphs from overlapping the next row.
    for (std::size_t i = 0; i < ruler_rows_.size(); ++i) {
      const auto& row = ruler_rows_[i];
      const int row_y = kTableTop + static_cast<int>(i) * kRowHeight;
      const bool active =
          last_distance_ > 0.0 &&
          std::fabs(row.distance - last_distance_) < 0.5;
      if (active) {
        RECT highlight{8, row_y, kOverlayWidth - 8, row_y + kRowHeight - 4};
        FillRect(dc, &highlight, highlight_brush_);
      }
      std::wostringstream distance_text;
      distance_text << std::fixed << std::setprecision(0) << row.distance
                    << L"m";
      DrawLine(dc, 16, row_y + 9, distance_text.str(),
               active ? amber : white, body_font_);
      std::wostringstream angle_text;
      angle_text << std::fixed << std::setprecision(1) << row.angle << L"°";
      DrawLine(dc, 118, row_y + 2, angle_text.str(), active ? green : white,
               ruler_font_);
      if (active) {
        DrawLine(dc, 236, row_y + 13, L"← 当前", green, small_font_);
      }
    }

    DrawLine(dc, 12, kOverlayHeight - 30,
             L"F8 输入距离  F9 显示/隐藏  Ctrl+Shift+Q 退出", muted,
             small_font_);

    HPEN border = CreatePen(PS_SOLID, 2, RGB(90, 120, 170));
    HGDIOBJ old_pen = SelectObject(dc, border);
    HGDIOBJ old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Rectangle(dc, 1, 1, rect.right - 1, rect.bottom - 1);
    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(border);
    EndPaint(hwnd, &paint);
  }

  HINSTANCE instance_ = nullptr;
  bool initialized_ = false;
  bool shutdown_ = false;
  HWND overlay_ = nullptr;
  HFONT title_font_ = nullptr;
  HFONT body_font_ = nullptr;
  HFONT ruler_font_ = nullptr;
  HFONT small_font_ = nullptr;
  HBRUSH background_brush_ = nullptr;
  HBRUSH highlight_brush_ = nullptr;
  std::wstring ini_path_;
  std::wstring text_input_;
  std::wstring status_message_;
  enum Mode { kNormal, kInput };
  Mode mode_ = kNormal;
  bool ruler_visible_ = false;
  double last_distance_ = 0.0;
  std::array<RulerRow, 6> ruler_rows_ = kRuler;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
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
