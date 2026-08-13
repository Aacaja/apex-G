#include "../original/arc_core.h"

#include <cmath>
#include <iomanip>
#include <iostream>

int main() {
  const auto angle = arcassist::HighArcElevationDegrees(30.0);
  if (!angle || std::abs(*angle - 75.0) > 1e-9) {
    std::cerr << "BASELINE_FAIL expected=75.0000\n";
    return 1;
  }
  std::cout << std::fixed << std::setprecision(4)
            << "BASELINE_OK distance=30.0000 high_arc=" << *angle << "\n";
  return 0;
}
