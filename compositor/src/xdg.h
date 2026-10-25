#pragma once
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <cstdint>
#include <string>
#include <vector>
#include "xdg-shell-server.h"
#include "util.h"
#include "wl.h"

namespace oasis {

class Server;
class Surface;
class XdgSurface;

// xdg_positioner: describes where a popup wants to appear relative to its
// parent and how to adjust when that would overflow the output.
class Positioner : public ResourceBound {
 public:
  Rect anchor_rect;
  int32_t w = 0, h = 0;
  uint32_t anchor = 0, gravity = 0, constraint = 0;
  int32_t ox = 0, oy = 0;

  // Resolves to the popup's top-left corner in layout space, given the
  // parent surface's layout-space origin and output bounds.
  void resolve(int32_t parent_lx, int32_t parent_ly, int32_t out_w,
               int32_t out_h, int32_t* lx, int32_t* ly) const;

  void set_size(wl_resource*, int32_t w_, int32_t h_) {
    w = w_;
    h = h_;
  }
  void set_anchor_rect(wl_resource*, int32_t x, int32_t y, int32_t w_,
                       int32_t h_) {
    anchor_rect = {x, y, w_, h_};
  }
  void set_anchor(wl_resource*, uint32_t a) { anchor = a; }
  void set_gravity(wl_resource*, uint32_t g) { gravity = g; }
  void set_constraint(wl_resource*, uint32_t c) { constraint = c; }
  void set_offset(wl_resource*, int32_t x, int32_t y) {
    ox = x;
    oy = y;
  }
  void set_reactive(wl_resource*) {}
  void set_parent_size(wl_resource*, int32_t, int32_t) {}
  void set_parent_configure(wl_resource*, uint32_t) {}
};

class XdgPopup : public ResourceBound {
 public:
  XdgPopup(Server* server, XdgSurface* xdg, Surface* parent);
  ~XdgPopup();

  Server* server;
  XdgSurface* xdg;
  Surface* surface;
  Surface* parent;            // parent wl_surface
  int32_t lx = 0, ly = 0;     // layout position
  bool grabbed = false;

  void grab(wl_resource*, wl_resource* seat_res, uint32_t serial);
  void reposition(wl_resource*, wl_resource* positioner_res, uint32_t token);
};

class XdgToplevel : public ResourceBound {
 public:
  XdgToplevel(Server* server, XdgSurface* xdg);
  ~XdgToplevel();

  Server* server;
  XdgSurface* xdg;
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
  std::vector<uint32_t> states() const;

  void set_parent(wl_resource*, wl_resource*) {}
  void set_title(wl_resource*, const char* t);
  void set_app_id(wl_resource*, const char* a);
  void show_window_menu(wl_resource*, wl_resource*, uint32_t, int32_t,
                        int32_t) {}
  void move(wl_resource*, wl_resource*, uint32_t);
  void resize(wl_resource*, wl_resource*, uint32_t, uint32_t edges);
  void set_max_size(wl_resource*, int32_t, int32_t) {}
  void set_min_size(wl_resource*, int32_t, int32_t) {}
  void set_maximized(wl_resource*);
  void unset_maximized(wl_resource*);
  void set_fullscreen(wl_resource*, wl_resource*);
  void unset_fullscreen(wl_resource*);
  void set_minimized(wl_resource*) {}
};

class XdgSurface : public ResourceBound {
 public:
  XdgSurface(Server* server, Surface* surface);
  ~XdgSurface();

  Server* server;
  Surface* surface;
  XdgToplevel* toplevel = nullptr;
  XdgPopup* popup = nullptr;
  Rect geometry;
  bool has_geometry = false;
  uint32_t pending_serial = 0;

  void committed();

  void get_toplevel(wl_client* client, wl_resource* res, uint32_t id);
  void get_popup(wl_client* client, wl_resource* res, uint32_t id,
                 wl_resource* parent_res, wl_resource* positioner_res);
  void set_window_geometry(wl_resource*, int32_t x, int32_t y, int32_t w,
                           int32_t h) {
    geometry = {x, y, w, h};
    has_geometry = true;
  }
  void ack_configure(wl_resource*, uint32_t serial) {
    if (serial == pending_serial) pending_serial = 0;
  }
};

// xdg_wm_base
class XdgShell {
 public:
  explicit XdgShell(Server* server);
  Server* server;

  void destroy(wl_resource* res) { wl_resource_destroy(res); }
  void create_positioner(wl_client* client, wl_resource* res, uint32_t id);
  void get_xdg_surface(wl_client* client, wl_resource* res, uint32_t id,
                       wl_resource* surface_res);
  void pong(wl_resource*, uint32_t) {}

 private:
  Global global_;
};

}  // namespace oasis
