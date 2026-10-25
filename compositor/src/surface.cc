#include "surface.h"

#include <algorithm>
#include <cstring>
#include "log.h"
#include "server.h"
#include "seat.h"
#include "xdg.h"

namespace oasis {

// ---------------------------------------------------------------- wl_region

static void region_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void region_add(wl_client*, wl_resource* res, int32_t x, int32_t y,
                       int32_t w, int32_t h) {
  auto* r = static_cast<Region*>(wl_resource_get_user_data(res));
  r->rects.push_back({x, y, w, h});
}

static void region_subtract(wl_client*, wl_resource* res, int32_t x, int32_t y,
                            int32_t w, int32_t h) {
  auto* r = static_cast<Region*>(wl_resource_get_user_data(res));
  Rect hole{x, y, w, h};
  std::vector<Rect> out;
  for (const Rect& q : r->rects) {
    if (q.x >= hole.x + hole.w || q.x + q.w <= hole.x ||
        q.y >= hole.y + hole.h || q.y + q.h <= hole.y) {
      out.push_back(q);
      continue;
    }
    if (q.y < hole.y) out.push_back({q.x, q.y, q.w, hole.y - q.y});
    if (q.y + q.h > hole.y + hole.h)
      out.push_back({q.x, hole.y + hole.h, q.w,
                     q.y + q.h - (hole.y + hole.h)});
    if (q.x < hole.x)
      out.push_back(
          {q.x, std::max(q.y, hole.y), hole.x - q.x,
           std::min(q.y + q.h, hole.y + hole.h) - std::max(q.y, hole.y)});
    if (q.x + q.w > hole.x + hole.w)
      out.push_back({hole.x + hole.w, std::max(q.y, hole.y),
                     q.x + q.w - (hole.x + hole.w),
                     std::min(q.y + q.h, hole.y + hole.h) -
                         std::max(q.y, hole.y)});
  }
  r->rects = std::move(out);
}

static const struct wl_region_interface region_impl = {
    region_destroy, region_add, region_subtract,
};

static void region_resource_destroy(wl_resource* res) {
  delete static_cast<Region*>(wl_resource_get_user_data(res));
}

// --------------------------------------------------------------- wl_surface

static void surface_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void surface_attach(wl_client*, wl_resource* res,
                           wl_resource* buffer_res, int32_t dx, int32_t dy) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  s->pending.has_buffer = true;
  s->pending.buffer_res = buffer_res;
  s->pending.dx = dx;
  s->pending.dy = dy;
}

static void surface_damage(wl_client*, wl_resource*, int32_t, int32_t, int32_t,
                           int32_t) {}

static void surface_damage_buffer(wl_client*, wl_resource*, int32_t, int32_t,
                                  int32_t, int32_t) {}

static void surface_frame(wl_client*, wl_resource* res, uint32_t cb_id) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  wl_resource* cb = wl_resource_create(wl_resource_get_client(res),
                                       &wl_callback_interface, 1, cb_id);
  s->pending.cbs.push_back(cb);
}

static void surface_set_opaque_region(wl_client*, wl_resource*, wl_resource*) {}

static void surface_set_input_region(wl_client*, wl_resource* res,
                                     wl_resource* region_res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  s->pending.has_input = true;
  if (region_res) {
    s->pending.input = *static_cast<Region*>(
        wl_resource_get_user_data(region_res));
    s->pending.input.infinite = false;
  } else {
    s->pending.input = {};
    s->pending.input.infinite = true;
  }
}

static void surface_commit(wl_client*, wl_resource* res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  s->commit();
}

static void surface_set_buffer_transform(wl_client*, wl_resource* res,
                                         int32_t transform) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  if (transform < 0 || transform > 7) {
    wl_resource_post_error(res, WL_SURFACE_ERROR_INVALID_TRANSFORM,
                           "invalid transform %d", transform);
    return;
  }
  s->pending.transform = transform;
}

static void surface_set_buffer_scale(wl_client*, wl_resource* res,
                                     int32_t scale) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  if (scale <= 0) {
    wl_resource_post_error(res, WL_SURFACE_ERROR_INVALID_SCALE,
                           "invalid scale %d", scale);
    return;
  }
  s->pending.scale = scale;
}

static void surface_offset(wl_client*, wl_resource* res, int32_t dx,
                           int32_t dy) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  if (s->pending.has_buffer) {
    s->pending.dx = dx;
    s->pending.dy = dy;
  }
}

static const struct wl_surface_interface surface_impl = {
    surface_destroy,           surface_attach,
    surface_damage,            surface_frame,
    surface_set_opaque_region, surface_set_input_region,
    surface_commit,            surface_set_buffer_transform,
    surface_set_buffer_scale,  surface_damage_buffer,
    surface_offset,
};

// ------------------------------------------------------------------ Surface

// Implemented in shm.cc / dmabuf.cc: wraps a wl_buffer resource as a Buffer.
Buffer* buffer_from_resource(wl_resource* res);
void buffer_take(Surface* s, wl_resource* res);

Surface::Surface(Server* srv, wl_resource* r) : server(srv), res(r) {
  input_region.infinite = true;
  pending.scale = 1;
  pending.transform = WL_OUTPUT_TRANSFORM_NORMAL;
}

Surface::~Surface() {
  for (wl_resource* cb : frame_callbacks) wl_resource_destroy(cb);
  for (wl_resource* cb : pending.cbs) wl_resource_destroy(cb);
  for (wl_resource* cb : cached.cbs) wl_resource_destroy(cb);
  auto& v = server->surfaces;
  v.erase(std::remove(v.begin(), v.end(), this), v.end());
}

static void surface_resource_destroy(wl_resource* res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  Server* server = s->server;
  if (server->cursor_surface == s) server->cursor_surface = nullptr;
  if (server->seat->pointer_focus == s) server->seat->pointer_focus = nullptr;
  if (server->seat->keyboard_focus == s)
    server->seat->keyboard_focus = nullptr;
  if (s->xdg) {
    if (s->xdg->toplevel) server->remove_toplevel(s->xdg->toplevel);
    s->xdg->surface = nullptr;
    s->xdg = nullptr;
  }
  for (XdgPopup* p : server->popups)
    if (p->parent == s) p->parent = nullptr;
  if (server->drag_target_surface == s) server->drag_target_surface = nullptr;
  if (s->parent) {
    auto& sib = s->parent->children;
    sib.erase(std::remove(sib.begin(), sib.end(), s), sib.end());
  }
  for (Surface* c : s->children) c->parent = nullptr;
  server->mark_dirty();
  server->schedule_windows_notify();
  delete s;
}

int32_t Surface::width() const {
  if (has_viewport && vp_dst_w > 0) return vp_dst_w;
  if (!buffer) return 0;
  int rot = buffer_transform & 1;
  int32_t w = rot ? buffer->height : buffer->width;
  if (has_viewport && vp_src_w > 0) w = (int32_t)vp_src_w;
  return w / buffer_scale;
}

int32_t Surface::height() const {
  if (has_viewport && vp_dst_h > 0) return vp_dst_h;
  if (!buffer) return 0;
  int rot = buffer_transform & 1;
  int32_t h = rot ? buffer->width : buffer->height;
  if (has_viewport && vp_src_h > 0) h = (int32_t)vp_src_h;
  return h / buffer_scale;
}

void Surface::apply_pending(Pending& p) {
  if (p.has_buffer) {
    if (p.buffer_res == nullptr) {
      destroy_buffer();
    } else if (buffer && buffer->res == p.buffer_res) {
      // re-attach of the same buffer, nothing to do
    } else {
      destroy_buffer();
      buffer_take(this, p.buffer_res);
      if (!buffer) {
        wl_resource_post_error(res, 0 /*invalid buffer*/,
                               "unusable buffer");
      }
    }
    buf_dx = p.dx;
    buf_dy = p.dy;
    p.has_buffer = false;
    p.buffer_res = nullptr;
  }
  if (p.has_input) {
    input_region = p.input;
    p.has_input = false;
  }
  buffer_scale = p.scale;
  buffer_transform = p.transform;
  if (p.has_viewport) {
    has_viewport = true;
    vp_src_x = p.vp_src_x;
    vp_src_y = p.vp_src_y;
    vp_src_w = p.vp_src_w;
    vp_src_h = p.vp_src_h;
    vp_dst_w = p.vp_dst_w;
    vp_dst_h = p.vp_dst_h;
    p.has_viewport = false;
  }
  for (wl_resource* cb : p.cbs) frame_callbacks.push_back(cb);
  p.cbs.clear();
  server->mark_dirty();
}

void Surface::destroy_buffer() {
  if (!buffer) return;
  if (buffer->res) {
    wl_buffer_send_release(buffer->res);
    buffer->res = nullptr;
  }
  buffer.reset();
}

void xdg_surface_committed(Surface* s);  // xdg.cc

void Surface::commit() {
  if (parent && sync) {
    cached = pending;
    has_cached = true;
    pending = Pending{};
    pending.scale = buffer_scale;
    pending.transform = buffer_transform;
    return;
  }
  apply_pending(pending);
  pending = Pending{};
  pending.scale = buffer_scale;
  pending.transform = buffer_transform;
  // A parent commit releases cached child state (sync subsurfaces).
  for (Surface* c : children)
    if (c->has_cached) {
      c->apply_pending(c->cached);
      c->has_cached = false;
      for (Surface* g : c->children)
        if (g->has_cached) {
          g->apply_pending(g->cached);
          g->has_cached = false;
        }
    }
  if (xdg) xdg_surface_committed(this);
}

void Surface::send_frame_done(uint32_t ms) {
  for (wl_resource* cb : frame_callbacks) {
    wl_callback_send_done(cb, ms);
    wl_resource_destroy(cb);
  }
  frame_callbacks.clear();
}

bool Surface::hit(double x, double y, double* sx, double* sy) {
  // children sit above their parent
  for (auto it = children.rbegin(); it != children.rend(); ++it) {
    Surface* c = *it;
    if (c->hit(x - c->sub_x, y - c->sub_y, sx, sy)) return true;
  }
  if (!buffer) return false;
  double lx = x + buf_dx, ly = y + buf_dy;
  if (lx < 0 || ly < 0 || lx >= width() || ly >= height()) return false;
  if (!input_region.contains(lx, ly)) return false;
  *sx = lx;
  *sy = ly;
  return true;
}

// ------------------------------------------------------------ wl_compositor

static void compositor_create_surface(wl_client* client, wl_resource* res,
                                      uint32_t id) {
  auto* server = static_cast<Server*>(wl_resource_get_user_data(res));
  wl_resource* sres =
      wl_resource_create(client, &wl_surface_interface,
                         wl_resource_get_version(res), id);
  auto* s = new Surface(server, sres);
  server->surfaces.push_back(s);
  wl_resource_set_implementation(sres, &surface_impl, s,
                                 surface_resource_destroy);
}

static void compositor_create_region(wl_client* client, wl_resource*,
                                     uint32_t id) {
  wl_resource* rres = wl_resource_create(client, &wl_region_interface, 1, id);
  wl_resource_set_implementation(rres, &region_impl, new Region(),
                                 region_resource_destroy);
}

static const struct wl_compositor_interface compositor_impl = {
    compositor_create_surface, compositor_create_region,
};

static void compositor_bind(wl_client* client, void* data, uint32_t version,
                            uint32_t id) {
  wl_resource* res =
      wl_resource_create(client, &wl_compositor_interface, version, id);
  wl_resource_set_implementation(res, &compositor_impl, data, nullptr);
}

void compositor_global_init(Server* server) {
  wl_global_create(server->display, &wl_compositor_interface, 5, server,
                   compositor_bind);
}

// ---------------------------------------------------------- wl_subcompositor

static void subsurface_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void subsurface_set_position(wl_client*, wl_resource* res, int32_t x,
                                    int32_t y) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  s->sub_x = x;
  s->sub_y = y;
  s->server->mark_dirty();
}

static void subsurface_place_above(wl_client*, wl_resource* res,
                                   wl_resource* sibling_res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  auto* sibling =
      static_cast<Surface*>(wl_resource_get_user_data(sibling_res));
  if (!s->parent || sibling->parent != s->parent) return;
  auto& kids = s->parent->children;
  kids.erase(std::remove(kids.begin(), kids.end(), s), kids.end());
  auto it = std::find(kids.begin(), kids.end(), sibling);
  kids.insert(it + 1, s);
  s->server->mark_dirty();
}

static void subsurface_place_below(wl_client*, wl_resource* res,
                                   wl_resource* sibling_res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  auto* sibling =
      static_cast<Surface*>(wl_resource_get_user_data(sibling_res));
  if (!s->parent || sibling->parent != s->parent) return;
  auto& kids = s->parent->children;
  kids.erase(std::remove(kids.begin(), kids.end(), s), kids.end());
  auto it = std::find(kids.begin(), kids.end(), sibling);
  kids.insert(it, s);
  s->server->mark_dirty();
}

static void subsurface_set_sync(wl_client*, wl_resource* res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  s->sync = true;
}

static void subsurface_set_desync(wl_client*, wl_resource* res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  s->sync = false;
}

static const struct wl_subsurface_interface subsurface_impl = {
    subsurface_destroy,     subsurface_set_position,
    subsurface_place_above, subsurface_place_below,
    subsurface_set_sync,    subsurface_set_desync,
};

static void subcompositor_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void subcompositor_get_subsurface(wl_client* client, wl_resource* res,
                                         uint32_t id,
                                         wl_resource* surface_res,
                                         wl_resource* parent_res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(surface_res));
  auto* parent =
      static_cast<Surface*>(wl_resource_get_user_data(parent_res));
  if (s == parent) {
    wl_resource_post_error(res, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "surface cannot be its own parent");
    return;
  }
  if (s->parent) {
    wl_resource_post_error(res, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "surface already has a parent");
    return;
  }
  wl_resource* sres =
      wl_resource_create(client, &wl_subsurface_interface,
                         wl_resource_get_version(res), id);
  s->subsurface_res = sres;
  s->parent = parent;
  s->sync = true;  // subsurfaces start in sync mode
  parent->children.push_back(s);
  wl_resource_set_implementation(sres, &subsurface_impl, s,
                                 [](wl_resource* r) {
                                   auto* s = static_cast<Surface*>(
                                       wl_resource_get_user_data(r));
                                   s->subsurface_res = nullptr;
                                   if (s->parent) {
                                     auto& sib = s->parent->children;
                                     sib.erase(std::remove(sib.begin(),
                                                           sib.end(), s),
                                               sib.end());
                                     s->parent = nullptr;
                                     s->server->mark_dirty();
                                   }
                                 });
}

static const struct wl_subcompositor_interface subcompositor_impl = {
    subcompositor_destroy, subcompositor_get_subsurface,
};

static void subcompositor_bind(wl_client* client, void*, uint32_t version,
                               uint32_t id) {
  wl_resource* res =
      wl_resource_create(client, &wl_subcompositor_interface, version, id);
  wl_resource_set_implementation(res, &subcompositor_impl, nullptr, nullptr);
}

void subcompositor_global_init(Server* server) {
  wl_global_create(server->display, &wl_subcompositor_interface, 1, nullptr,
                   subcompositor_bind);
}

}  // namespace oasis
