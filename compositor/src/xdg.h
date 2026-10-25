#pragma once
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <cstdint>
#include <string>
#include <vector>
#include "xdg-shell-server.h"
#include "util.h"

namespace oasis {

class Server;
class Surface;
class XdgSurface;

struct Positioner {
  Rect anchor_rect;
  int32_t w = 0, h = 0;
  uint32_t anchor = 0, gravity = 0, constraint = 0;
  int32_t ox = 0, oy = 0;
  // Resolves to surface-local coordinates of the popup's top-left corner,
  // given the parent surface's layout-space origin and output bounds.
  void resolve(int32_t parent_lx, int32_t parent_ly, int32_t out_w,
               int32_t out_h, int32_t* lx, int32_t* ly) const;
};

class XdgPopup {
 public:
  Server* server;
  wl_resource* res = nullptr;
  Surface* surface = nullptr;
  Surface* parent = nullptr;      // parent wl_surface
  int32_t lx = 0, ly = 0;         // layout position
  bool grabbed = false;
};

class XdgToplevel {
 public:
  Server* server;
  wl_resource* res = nullptr;
  XdgSurface* xdg = nullptr;
  uint32_t id = 0;
  std::string title, app_id;
  int32_t x = 0, y = 0;  // layout position of surface origin
  bool activated = false, maximized = false, fullscreen = false;
  int32_t saved_x = 0, saved_y = 0;
  uint32_t configure_serial = 0;
  bool configure_sent = false;

  Surface* surface() const;
  void send_configure(int32_t w, int32_t h,
                      const std::vector<uint32_t>& states);
  Rect bounds() const;
};

class XdgSurface {
 public:
  Server* server;
  wl_resource* res = nullptr;
  Surface* surface = nullptr;
  XdgToplevel* toplevel = nullptr;
  XdgPopup* popup = nullptr;
  Rect geometry;
  bool has_geometry = false;
  uint32_t pending_serial = 0;
};

}  // namespace oasis
