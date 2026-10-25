#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace oasis {

struct Rect {
  int32_t x = 0, y = 0, w = 0, h = 0;
  bool empty() const { return w <= 0 || h <= 0; }
  bool contains(double px, double py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
  Rect intersected(const Rect& o) const {
    int32_t x1 = std::max(x, o.x), y1 = std::max(y, o.y);
    int32_t x2 = std::min(x + w, o.x + o.w), y2 = std::min(y + h, o.y + o.h);
    return {x1, y1, std::max(0, x2 - x1), std::max(0, y2 - y1)};
  }
};

// A union of rectangles, or the infinite region. Mirrors wl_region semantics
// closely enough for hit-testing and opaque regions.
struct Region {
  std::vector<Rect> rects;
  bool infinite = false;

  bool contains(double x, double y) const {
    if (infinite) return true;
    for (const auto& r : rects)
      if (r.contains(x, y)) return true;
    return false;
  }
  Rect extents() const {
    if (rects.empty()) return {};
    int32_t x1 = rects[0].x, y1 = rects[0].y, x2 = x1 + rects[0].w,
            y2 = y1 + rects[0].h;
    for (const auto& r : rects) {
      x1 = std::min(x1, r.x);
      y1 = std::min(y1, r.y);
      x2 = std::max(x2, r.x + r.w);
      y2 = std::max(y2, r.y + r.h);
    }
    return {x1, y1, x2 - x1, y2 - y1};
  }
};

uint32_t now_msec();
uint64_t now_usec();
int memfd_create_cloexec(const char* name);
bool write_all(int fd, const void* data, size_t size);
std::string read_fd_to_string(int fd);

}  // namespace oasis
