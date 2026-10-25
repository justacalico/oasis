#include "server.h"

#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "flutter.h"
#include "log.h"
#include "renderer.h"
#include "seat.h"
#include "surface.h"
#include "xdg.h"

namespace oasis {

void compositor_global_init(Server*);
void subcompositor_global_init(Server*);
void shm_global_init(Server*);
void xdg_global_init(Server*);
void output_global_init(Server*, Backend::Output*);
void data_device_global_init(Server*);
void dmabuf_global_init(Server*);
void decoration_global_init(Server*);
void viewporter_global_init(Server*);

static uint32_t g_toplevel_id = 0;

// Procedural arrow cursor (32x32), white fill with dark outline.
uint32_t cursor_arrow_pixels[32 * 32];
void build_cursor() {
  static const char* shape[22] = {
      "X                    ",
      "XX                   ",
      "XaX                  ",
      "XaaX                 ",
      "XaaaX                ",
      "XaaaaX               ",
      "XaaaaaX              ",
      "XaaaaaaX             ",
      "XaaaaaaaX            ",
      "XaaaaaaaaX           ",
      "XaaaaaaaaaX          ",
      "XaaaaaaaaaaX         ",
      "XaaaaaaXXXXX         ",
      "XaaaXaaX             ",
      "XaaX XaaX            ",
      "XaX  XaaX            ",
      "XX    XaaX           ",
      "X      XaaX          ",
      "        XaX          ",
      "        XaX          ",
      "         XX          ",
      "                     "};
  memset(cursor_arrow_pixels, 0, sizeof(cursor_arrow_pixels));
  for (int y = 0; y < 22; y++)
    for (int x = 0; x < 22; x++) {
      char c = shape[y][x];
      if (c == 'X')
        cursor_arrow_pixels[y * 32 + x] = 0xff1c1c1e;
      else if (c == 'a')
        cursor_arrow_pixels[y * 32 + x] = 0xfff5f5f7;
    }
}

Backend::Output* Server::primary_output() {
  if (!backend || backend->outputs().empty()) return nullptr;
  return backend->outputs()[0];
}

void Server::schedule_windows_notify() {
  if (windows_idle || !flutter || !flutter->running()) return;
  windows_idle = wl_event_loop_add_idle(
      loop,
      [](void* data) {
        auto* s = static_cast<Server*>(data);
        s->windows_idle = nullptr;
        s->flutter->notify_windows();
      },
      this);
}

void Server::raise(XdgToplevel* t) {
  auto& v = toplevels;
  auto it = std::find(v.begin(), v.end(), t);
  if (it == v.end()) return;
  v.erase(it);
  v.push_back(t);
  if (!t->activated) {
    t->activated = true;
    std::vector<uint32_t> states = {XDG_TOPLEVEL_STATE_ACTIVATED};
    if (t->maximized) states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
    if (t->fullscreen) states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
    t->send_configure(t->surface() ? t->surface()->width() : 0,
                      t->surface() ? t->surface()->height() : 0, states);
  }
  // deactivate the previously topmost window
  for (XdgToplevel* o : v) {
    if (o != t && o->activated) {
      o->activated = false;
      std::vector<uint32_t> states;
      if (o->maximized) states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
      if (o->fullscreen) states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
      o->send_configure(0, 0, states);
    }
  }
  mark_dirty();
  schedule_windows_notify();
}

void Server::add_toplevel(XdgToplevel* t) {
  LOGI("toplevel mapped: %s (%s)", t->title.c_str(), t->app_id.c_str());
  if (t->id == 0) t->id = ++g_toplevel_id;
  if (!t->maximized && !t->fullscreen) {
    size_t n = toplevels.size();
    Backend::Output* o = primary_output();
    int32_t ox = o ? o->x : 0, oy = o ? o->y : 0;
    t->x = ox + 80 + (int32_t)(n % 8) * 40;
    t->y = oy + 56 + (int32_t)(n % 8) * 36;
  }
  toplevels.push_back(t);
  raise(t);
}

void Server::remove_toplevel(XdgToplevel* t) {
  auto& v = toplevels;
  v.erase(std::remove(v.begin(), v.end(), t), v.end());
  if (seat && seat->keyboard_focus == t->surface())
    seat->set_keyboard_focus(nullptr);
  mark_dirty();
  schedule_windows_notify();
}

bool Server::surface_contains(Surface* s, double x, double y, double* sx,
                              double* sy) {
  return s->hit(x, y, sx, sy);
}

Surface* Server::surface_at(double x, double y, Surface** out_surface,
                            double* sx, double* sy) {
  // popups sit above toplevels
  for (auto it = popups.rbegin(); it != popups.rend(); ++it) {
    XdgPopup* p = *it;
    if (!p->surface) continue;
    double lx = 0, ly = 0;
    if (p->surface->hit(x - p->lx, y - p->ly, &lx, &ly)) {
      *sx = lx;
      *sy = ly;
      *out_surface = p->surface;
      return p->surface;
    }
  }
  for (auto it = toplevels.rbegin(); it != toplevels.rend(); ++it) {
    XdgToplevel* t = *it;
    Surface* s = t->surface();
    if (!s) continue;
    double lx = 0, ly = 0;
    if (s->hit(x - t->x, y - t->y, &lx, &ly)) {
      *sx = lx;
      *sy = ly;
      *out_surface = s;
      return s;
    }
  }
  *out_surface = nullptr;
  return nullptr;
}

void Server::draw_surface_tree(Renderer* r, Surface* s, int32_t ox,
                               int32_t oy) {
  // children below the parent's own content? wlroots draws subsurfaces above
  // the parent in stacking order; below-parent placement is rare, so render
  // parent first then children in order.
  if (s->buffer) {
    GLuint tex = s->buffer->texture(r);
    int32_t w = s->width(), h = s->height();
    if (s->has_viewport && (s->vp_dst_w > 0 || s->vp_src_w > 0)) {
      w = s->vp_dst_w > 0 ? s->vp_dst_w : (int32_t)s->vp_src_w;
      h = s->vp_dst_h > 0 ? s->vp_dst_h : (int32_t)s->vp_src_h;
    }
    r->texture(tex, ox + s->buf_dx, oy + s->buf_dy, w, h,
               s->buffer_transform, s->buffer->opaque());
    s->send_frame_done(now_msec());
  }
  for (Surface* c : s->children)
    draw_surface_tree(r, c, ox + c->sub_x, oy + c->sub_y);
}

void Server::render(Backend::Output* o) {
  if (!dirty || !o || !renderer) return;
  dirty = false;

  backend->begin_frame(o);
  if (!renderer_ready_) {
    renderer->init();
    renderer_ready_ = true;
  }
  if (!cursor_theme_tex) {
    cursor_theme_tex =
        renderer->texture_from_pixels(32, 32, cursor_arrow_pixels);
    cursor_theme_w = 32;
    cursor_theme_h = 32;
    cursor_theme_hx = 2;
    cursor_theme_hy = 2;
  }
  renderer->begin(o->width, o->height);
  renderer->clear(0.05f, 0.05f, 0.08f, 1.0f);

  GLuint shell_tex = flutter ? flutter->shell_texture() : 0;
  if (shell_tex)
    renderer->texture(shell_tex, o->x, o->y, o->width, o->height, 0, false);

  for (XdgToplevel* t : toplevels) {
    Surface* s = t->surface();
    if (s) draw_surface_tree(renderer, s, t->x, t->y);
  }
  for (XdgPopup* p : popups)
    if (p->surface) draw_surface_tree(renderer, p->surface, p->lx, p->ly);

  // cursor
  int32_t cx = (int32_t)cursor_x, cy = (int32_t)cursor_y;
  if (cursor_surface && cursor_surface->buffer) {
    Surface* cs = cursor_surface;
    GLuint tex = cs->buffer->texture(renderer);
    renderer->texture(tex, cx - cs->buf_dx - cursor_hx,
                      cy - cs->buf_dy - cursor_hy, cs->width(), cs->height(),
                      cs->buffer_transform, cs->buffer->opaque());
  } else if (cursor_theme_tex) {
    renderer->texture(cursor_theme_tex, cx - cursor_theme_hx,
                      cy - cursor_theme_hy, cursor_theme_w, cursor_theme_h, 0,
                      true);
  }

  backend->end_frame(o);
}

// ------------------------------------------------------------------ wl_output

static void output_bind_resource(wl_client* client, void* data,
                                 uint32_t version, uint32_t id) {
  auto* out = static_cast<Backend::Output*>(data);
  version = std::min<uint32_t>(version, 4);
  wl_resource* res =
      wl_resource_create(client, &wl_output_interface, version, id);
  wl_resource_set_implementation(res, nullptr, out, [](wl_resource* r) {
    auto* out = static_cast<Backend::Output*>(wl_resource_get_user_data(r));
    auto& v = out->resources;
    v.erase(std::remove(v.begin(), v.end(), r), v.end());
  });
  out->resources.push_back(res);
  wl_output_send_geometry(res, out->x, out->y, out->phys_w, out->phys_h,
                          WL_OUTPUT_SUBPIXEL_UNKNOWN, out->make.c_str(),
                          out->model.c_str(), WL_OUTPUT_TRANSFORM_NORMAL);
  if (version >= WL_OUTPUT_NAME_SINCE_VERSION)
    wl_output_send_name(res, out->name.c_str());
  if (version >= WL_OUTPUT_DESCRIPTION_SINCE_VERSION)
    wl_output_send_description(res, out->model.c_str());
  wl_output_send_mode(res, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                      out->width, out->height, out->refresh);
  if (version >= WL_OUTPUT_SCALE_SINCE_VERSION)
    wl_output_send_scale(res, out->scale);
  wl_output_send_done(res);
}

void output_global_init(Server* server, Backend::Output* out) {
  wl_global_create(server->display, &wl_output_interface, 4, out,
                   output_bind_resource);
}

void output_send_mode_changed(Backend::Output* out) {
  for (wl_resource* res : out->resources) {
    wl_output_send_mode(res, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                        out->width, out->height, out->refresh);
    wl_output_send_done(res);
  }
}

}  // namespace oasis

// ------------------------------------------------------------ init & run

namespace oasis {

bool Server::init(const Config& cfg) {
  display = wl_display_create();
  if (!display) return false;
  loop = wl_display_get_event_loop(display);

  const char* host_display = getenv("WAYLAND_DISPLAY");
  bool nested = !cfg.force_drm && host_display && host_display[0];
  if (nested) setenv("OASIS_HOST_DISPLAY", host_display, 1);

  const char* sock = nullptr;
  if (cfg.socket.empty()) {
    sock = wl_display_add_socket_auto(display);
  } else {
    if (wl_display_add_socket(display, cfg.socket.c_str()) == 0)
      sock = cfg.socket.c_str();
  }
  if (!sock) {
    LOGE("cannot create wayland socket");
    return false;
  }
  socket_name = sock;
  setenv("WAYLAND_DISPLAY", socket_name.c_str(), 1);
  setenv("OASIS_COMPOSITOR", "1", 1);
  unsetenv("DISPLAY");
  LOGI("listening on wayland socket %s", socket_name.c_str());

  backend = nested ? create_wayland_backend() : create_drm_backend();
  if (!backend) return false;

  static wl_listener client_listener = {};
  client_listener.notify = [](wl_listener*, void* data) {
    LOGI("client connected: %p", data);
  };
  wl_display_add_client_created_listener(display, &client_listener);

  seat = new Seat(this);
  seat->init();

  Backend::Callbacks cbs;
  cbs.frame = [this](Backend::Output* o) { render(o); };
  cbs.pointer_motion = [this](double x, double y, uint32_t t) {
    seat->motion(x, y, t);
    if (dragging) dnd_motion(x, y, t);
  };
  cbs.pointer_button = [this](uint32_t b, bool p, uint32_t t) {
    seat->button(b, p, t);
    if (dragging && !p) dnd_drop(t);
  };
  cbs.pointer_axis = [this](double dx, double dy, uint32_t t) {
    seat->axis(dx, dy, t);
  };
  cbs.key = [this](uint32_t k, bool p, uint32_t t) { seat->key(k, p, t); };
  cbs.modifiers = [this](uint32_t d, uint32_t l, uint32_t lk, uint32_t g) {
    seat->modifiers(d, l, lk, g);
  };
  cbs.keymap = [this](const std::string& s) { seat->set_keymap(s); };
  cbs.output_resized = [this](Backend::Output* o, int32_t w, int32_t h) {
    output_send_mode_changed(o);
    if (flutter) flutter->metrics(w, h, (double)o->scale);
    mark_dirty();
  };
  if (!backend->init(loop, std::move(cbs))) return false;

  renderer = new Renderer();

  compositor_global_init(this);
  subcompositor_global_init(this);
  shm_global_init(this);
  xdg_global_init(this);
  data_device_global_init(this);
  decoration_global_init(this);
  viewporter_global_init(this);
  for (Backend::Output* o : backend->outputs()) output_global_init(this, o);
  dmabuf_global_init(this);

  flutter = new FlutterHost();
  if (!cfg.bundle_dir.empty() &&
      !flutter->init(this, cfg.bundle_dir.c_str(), cfg.engine_dir.c_str())) {
    LOGE("flutter engine failed to start; shell UI will be blank");
  } else if (cfg.bundle_dir.empty()) {
    LOGI("no flutter bundle; running compositor only");
  }
  if (flutter->running()) {
    Backend::Output* o = primary_output();
    if (o) flutter->metrics(o->width, o->height, (double)o->scale);
    schedule_windows_notify();
  }
  build_cursor();
  return true;
}

void Server::run() {
  while (running) {
    if (wl_event_loop_dispatch(loop, -1) < 0) break;
    wl_display_flush_clients(display);
  }
}

}  // namespace oasis
