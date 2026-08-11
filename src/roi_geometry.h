#pragma once

#include <algorithm>

namespace arcassist {

constexpr int kMinRoiWidth = 24;
constexpr int kMinRoiHeight = 10;
constexpr int kMaxRoiWidth = 240;
constexpr int kMaxRoiHeight = 80;
constexpr int kDefaultRoiWidth = 72;
constexpr int kDefaultRoiHeight = 18;
constexpr int kQuickResizeWidthStep = 8;
constexpr int kQuickResizeHeightStep = 4;

struct RoiGeometry {
  int x = 48;
  int y = 36;
  int w = kDefaultRoiWidth;
  int h = kDefaultRoiHeight;
};

inline RoiGeometry ResizeRoiCentered(RoiGeometry roi, int width_delta,
                                     int height_delta) {
  const int old_width = roi.w;
  const int old_height = roi.h;
  roi.w = std::clamp(roi.w + width_delta, kMinRoiWidth, kMaxRoiWidth);
  roi.h = std::clamp(roi.h + height_delta, kMinRoiHeight, kMaxRoiHeight);
  roi.x += (old_width - roi.w) / 2;
  roi.y += (old_height - roi.h) / 2;
  return roi;
}

inline RoiGeometry CompactRoiCentered(RoiGeometry roi) {
  return ResizeRoiCentered(roi, kDefaultRoiWidth - roi.w,
                           kDefaultRoiHeight - roi.h);
}

inline RoiGeometry ClampRoiToBounds(RoiGeometry roi, int left, int top,
                                    int right, int bottom) {
  if (right <= left || bottom <= top) return roi;
  const int bounds_width = right - left;
  const int bounds_height = bottom - top;
  roi.w = std::clamp(roi.w, 1, bounds_width);
  roi.h = std::clamp(roi.h, 1, bounds_height);
  roi.x = std::clamp(roi.x, left, right - roi.w);
  roi.y = std::clamp(roi.y, top, bottom - roi.h);
  return roi;
}

}  // namespace arcassist
