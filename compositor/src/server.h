#pragma once
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include "backend.h"
#include "util.h"

namespace oasis {

class Renderer;
class Seat;
class FlutterHost;
class Surface;
class XdgToplevel;
class XdgPopup;

struct Config {
  std::string bundle_dir;
  std::string engine_dir;
  std::string socket;
  bool force_drm = false;
};

class Server {
 public:
  wl_display* display = nullptr;
  wl_event_loop* loop = nullptr;
  Backend* backend = nullptr;
  Renderer* renderer = nullptr;
  Seat* seat = nullptr;
  FlutterHost* flutter = nullptr;

  std::vector<Surface*> surfaces;
  std::vector<XdgToplevel*> toplevels;  // stacking order, bottom to top
  std::vector<XdgPopup*> popups;

  double cursor_x = 0, cursor_y = 0;
  Surface* cursor_surface = nullptr;
  int32_t cursor_hx = 0, cursor_hy = 0;
  uint32_t cursor_theme_tex = 0;
  int32_t cursor_theme_w = 0, cursor_theme_h = 0;
  int32_t cursor_theme_hx = 0, cursor_theme_hy = 0;

  std::atomic<bool> dirty{true};
  bool renderer_ready_ = false;
  bool running = true;
  wl_event_source* windows_idle = nullptr;

  // clipboard / dnd state (data_device.cc)
  std::vector<wl_resource*> data_devices;
  void* selection_source = nullptr;
  wl_client* selection_client = nullptr;
  bool dragging = false;
  void* drag_source = nullptr;
  Surface* drag_target_surface = nullptr;

  bool init(const Config& cfg);
  void run();
  std::string socket_name;
  void quit() { running = false; }

  void mark_dirty() { dirty = true; }
  void schedule_windows_notify();

  Backend::Output* primary_output();
  Surface* surface_at(double x, double y, Surface** out_surface,
                      double* sx, double* sy);
  bool surface_contains(Surface* s, double x, double y, double* sx,
                        double* sy);
  void raise(XdgToplevel* t);
  void add_toplevel(XdgToplevel* t);
  void remove_toplevel(XdgToplevel* t);
  void render(Backend::Output* o);
  void draw_surface_tree(Renderer* r, Surface* s, int32_t ox, int32_t oy);
  void notify_selection();
  void dnd_motion(double x, double y, uint32_t t);
  void dnd_drop(uint32_t t);
};

void output_send_mode_changed(Backend::Output* out);
void xdg_surface_committed(Surface* s);
void surface_layout_pos(Surface* s, int32_t* lx, int32_t* ly);
void server_popup_add(Server* server, XdgPopup* pop);
void server_popup_remove(Server* server, XdgPopup* pop);

}  // namespace oasis
