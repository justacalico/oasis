#pragma once
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <atomic>
#include <array>
#include <memory>
#include <string>
#include <vector>
#include "backend.h"
#include "flutter.h"
#include "protocols.h"
#include "renderer.h"
#include "seat.h"
#include "surface.h"
#include "util.h"
#include "wl.h"
#include "xdg.h"

namespace oasis {

struct Config {
  std::string bundle_dir;
  std::string engine_dir;
  std::string socket;
  bool force_drm = false;
};

class Server {
 public:
  ~Server();

  wl_display* display = nullptr;
  wl_event_loop* loop = nullptr;
  std::unique_ptr<Backend> backend;
  std::unique_ptr<Renderer> renderer;
  std::unique_ptr<Seat> seat;
  std::unique_ptr<FlutterHost> flutter;

  std::unique_ptr<Compositor> compositor;
  std::unique_ptr<Subcompositor> subcompositor;
  std::unique_ptr<Shm> shm;
  std::unique_ptr<XdgShell> shell;
  std::unique_ptr<DataDeviceManager> data_manager;
  std::unique_ptr<DecorationManager> decorations;
  std::unique_ptr<Viewporter> viewporter;
  std::unique_ptr<Dmabuf> dmabuf;
  std::vector<std::unique_ptr<OutputGlobal>> output_globals;

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

  bool init(const Config& cfg);
  void run();
  std::string socket_name;
  void quit() { running = false; }

  void mark_dirty() { dirty = true; }
  void schedule_windows_notify();

  Backend::Output* primary_output();
  Surface* surface_at(double x, double y, Surface** out_surface,
                      double* sx, double* sy);
  void raise(XdgToplevel* t);
  void add_toplevel(XdgToplevel* t);
  void remove_toplevel(XdgToplevel* t);
  void add_popup(XdgPopup* p) { popups.push_back(p); }
  void remove_popup(XdgPopup* p);
  void render(Backend::Output* o);
  void draw_surface_tree(Renderer* r, Surface* s, int32_t ox, int32_t oy);

 private:
  void build_cursor();
  void send_mode(Backend::Output* out);

  uint32_t next_toplevel_id_ = 0;
  std::array<uint32_t, 32 * 32> cursor_pixels_{};
  std::unique_ptr<EventSource> windows_idle_;
  Listener client_created_;
};

}  // namespace oasis
