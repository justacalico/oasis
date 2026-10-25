#include "server.h"

#include <unistd.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include "flutter.h"
#include "log.h"
#include "protocols.h"
#include "renderer.h"
#include "seat.h"
#include "surface.h"
#include "xdg.h"

namespace oasis {

Server::~Server() = default;

// Procedural arrow cursor (32x32), white fill with dark outline.
void Server::build_cursor() {
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
  cursor_pixels_.fill(0);
  for (int y = 0; y < 22; y++)
    for (int x = 0; x < 22; x++) {
      char c = shape[y][x];
      if (c == 'X')
        cursor_pixels_[y * 32 + x] = 0xff1c1c1e;
      else if (c == 'a')
        cursor_pixels_[y * 32 + x] = 0xfff5f5f7;
    }
}

Backend::Output* Server::primary_output() {
  if (!backend || backend->outputs().empty()) return nullptr;
  return backend->outputs()[0];
}

void Server::schedule_windows_notify() {
  if (windows_idle_ || !flutter || !flutter->running()) return;
  windows_idle_ = EventSource::when_idle(loop, [this] {
    windows_idle_.reset();
    flutter->notify_windows();
  });
}

void Server::raise(XdgToplevel* t) {
  auto& v = toplevels;
  auto it = std::find(v.begin(), v.end(), t);
  if (it == v.end()) return;
  v.erase(it);
  v.push_back(t);
  if (!t->activated) {
    t->activated = true;
    Surface* s = t->surface();
    t->send_configure(s ? s->width() : 0, s ? s->height() : 0, t->states());
  }
  // deactivate the previously topmost window
  for (XdgToplevel* o : v) {
    if (o != t && o->activated) {
      o->activated = false;
      o->send_configure(0, 0, o->states());
    }
  }
  mark_dirty();
  schedule_windows_notify();
}

void Server::add_toplevel(XdgToplevel* t) {
  LOGI("toplevel mapped: %s (%s)", t->title.c_str(), t->app_id.c_str());
  if (t->id == 0) t->id = ++next_toplevel_id_;
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

void Server::remove_popup(XdgPopup* p) {
  popups.erase(std::remove(popups.begin(), popups.end(), p), popups.end());
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
        renderer->texture_from_pixels(32, 32, cursor_pixels_.data());
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
    if (s) draw_surface_tree(renderer.get(), s, t->x, t->y);
  }
  for (XdgPopup* p : popups)
    if (p->surface) draw_surface_tree(renderer.get(), p->surface, p->lx, p->ly);

  // cursor
  int32_t cx = (int32_t)cursor_x, cy = (int32_t)cursor_y;
  if (cursor_surface && cursor_surface->buffer) {
    Surface* cs = cursor_surface;
    GLuint tex = cs->buffer->texture(renderer.get());
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

static const struct wl_output_interface output_impl = {destroy_request};

OutputGlobal::OutputGlobal(Server* server, Backend::Output* out) : out_(out) {
  global_.create(server->display, &wl_output_interface, 4,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   bind(c, v, id);
                 });
}

void OutputGlobal::bind(wl_client* client, uint32_t version, uint32_t id) {
  version = std::min<uint32_t>(version, 4);
  wl_resource* res =
      wl_resource_create(client, &wl_output_interface, version, id);
  wl_resource_set_implementation(res, &output_impl, out_,
                                 &OutputGlobal::resource_destroyed);
  out_->resources.push_back(res);
  wl_output_send_geometry(res, out_->x, out_->y, out_->phys_w, out_->phys_h,
                          WL_OUTPUT_SUBPIXEL_UNKNOWN, out_->make.c_str(),
                          out_->model.c_str(), WL_OUTPUT_TRANSFORM_NORMAL);
  if (version >= WL_OUTPUT_NAME_SINCE_VERSION)
    wl_output_send_name(res, out_->name.c_str());
  if (version >= WL_OUTPUT_DESCRIPTION_SINCE_VERSION)
    wl_output_send_description(res, out_->model.c_str());
  wl_output_send_mode(res, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                      out_->width, out_->height, out_->refresh);
  if (version >= WL_OUTPUT_SCALE_SINCE_VERSION)
    wl_output_send_scale(res, out_->scale);
  wl_output_send_done(res);
}

void OutputGlobal::resource_destroyed(wl_resource* res) {
  auto* out = user_data<Backend::Output>(res);
  auto& v = out->resources;
  v.erase(std::remove(v.begin(), v.end(), res), v.end());
}

void OutputGlobal::send_mode() {
  for (wl_resource* res : out_->resources) {
    wl_output_send_mode(res, WL_OUTPUT_MODE_CURRENT | WL_OUTPUT_MODE_PREFERRED,
                        out_->width, out_->height, out_->refresh);
    wl_output_send_done(res);
  }
}

void Server::send_mode(Backend::Output* out) {
  for (auto& og : output_globals)
    if (og->output() == out) og->send_mode();
}

// ------------------------------------------------------------ init & run

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

  client_created_.set(
      [](void* client) { LOGI("client connected: %p", client); });
  wl_display_add_client_created_listener(display, client_created_.native());

  seat = std::make_unique<Seat>(this);
  seat->init();

  Backend::Callbacks cbs;
  cbs.frame = [this](Backend::Output* o) { render(o); };
  cbs.pointer_motion = [this](double x, double y, uint32_t t) {
    seat->motion(x, y, t);
    if (data_manager) data_manager->dnd_motion(x, y, t);
  };
  cbs.pointer_button = [this](uint32_t b, bool p, uint32_t t) {
    seat->button(b, p, t);
    if (data_manager && !p) data_manager->dnd_drop(t);
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
    send_mode(o);
    if (flutter) flutter->metrics(w, h, (double)o->scale);
    mark_dirty();
  };
  cbs.quit = [this] { quit(); };
  if (!backend->init(loop, std::move(cbs))) return false;

  renderer = std::make_unique<Renderer>();

  compositor = std::make_unique<Compositor>(this);
  subcompositor = std::make_unique<Subcompositor>(this);
  shm = std::make_unique<Shm>(this);
  shell = std::make_unique<XdgShell>(this);
  data_manager = std::make_unique<DataDeviceManager>(this);
  decorations = std::make_unique<DecorationManager>(this);
  viewporter = std::make_unique<Viewporter>(this);
  for (Backend::Output* o : backend->outputs())
    output_globals.push_back(std::make_unique<OutputGlobal>(this, o));
  dmabuf = std::make_unique<Dmabuf>(this);

  flutter = std::make_unique<FlutterHost>();
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
