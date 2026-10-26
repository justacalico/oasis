#include "surface.h"

#include <algorithm>
#include <cstring>
#include "log.h"
#include "protocols.h"
#include "seat.h"
#include "server.h"
#include "xdg.h"

namespace oasis {

// ------------------------------------------------------------------ Surface

Surface::Surface(Server* srv) : server(srv) {
  input_region.infinite = true;
  pending.scale = 1;
  pending.transform = WL_OUTPUT_TRANSFORM_NORMAL;
}

Surface::~Surface() {
  for (wl_resource* cb : frame_callbacks) wl_resource_destroy(cb);
  for (wl_resource* cb : pending.cbs) wl_resource_destroy(cb);
  for (wl_resource* cb : cached.cbs) wl_resource_destroy(cb);
  if (server->cursor_surface == this) server->cursor_surface = nullptr;
  if (server->seat->pointer_focus == this)
    server->seat->pointer_focus = nullptr;
  if (server->seat->keyboard_focus == this)
    server->seat->keyboard_focus = nullptr;
  if (xdg) {
    if (xdg->toplevel) server->remove_toplevel(xdg->toplevel);
    xdg->surface = nullptr;
    xdg = nullptr;
  }
  for (XdgPopup* p : server->popups)
    if (p->parent == this) p->parent = nullptr;
  server->data_manager->clear_drag_target(this);
  if (subsurface) wl_resource_destroy(subsurface->res);
  if (parent) {
    auto& sib = parent->children;
    sib.erase(std::remove(sib.begin(), sib.end(), this), sib.end());
  }
  for (Surface* c : children) c->parent = nullptr;
  auto& v = server->surfaces;
  v.erase(std::remove(v.begin(), v.end(), this), v.end());
  server->mark_dirty();
  server->schedule_windows_notify();
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

void Surface::take_buffer(wl_resource* buffer_res) {
  auto* buf = Buffer::from(buffer_res);
  if (!buf) {
    wl_resource_post_error(res, 0 /*invalid buffer*/, "unknown buffer");
    return;
  }
  buf->taken = true;
  buffer.reset(buf);
}

void Surface::apply_pending(Pending& p) {
  if (p.has_buffer) {
    if (p.buffer_res == nullptr) {
      destroy_buffer();
    } else if (buffer && buffer->res == p.buffer_res) {
      // re-attach of the same buffer, nothing to do
    } else {
      destroy_buffer();
      take_buffer(p.buffer_res);
      if (!buffer) {
        wl_resource_post_error(res, 0 /*invalid buffer*/, "unusable buffer");
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
  if (xdg) xdg->committed();
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

void Surface::layout_pos(int32_t* lx, int32_t* ly) {
  int32_t x = 0, y = 0;
  Surface* s = this;
  while (s) {
    if (s->xdg && s->xdg->toplevel) {
      x += s->xdg->toplevel->x;
      y += s->xdg->toplevel->y;
      break;
    }
    bool popup_found = false;
    for (XdgPopup* p : s->server->popups)
      if (p->surface == s) {
        x += p->lx;
        y += p->ly;
        popup_found = true;
        break;
      }
    if (popup_found) break;
    if (s->parent) {
      x += s->sub_x;
      y += s->sub_y;
      s = s->parent;
    } else {
      break;
    }
  }
  *lx = x;
  *ly = y;
}

// wl_surface requests

void Surface::attach(wl_resource*, wl_resource* buffer_res, int32_t dx,
                     int32_t dy) {
  pending.has_buffer = true;
  pending.buffer_res = buffer_res;
  pending.dx = dx;
  pending.dy = dy;
}

void Surface::frame(wl_resource*, uint32_t cb_id) {
  wl_resource* cb = wl_resource_create(wl_resource_get_client(res),
                                     &wl_callback_interface, 1, cb_id);
  pending.cbs.push_back(cb);
}

void Surface::set_input_region(wl_resource*, wl_resource* region_res) {
  pending.has_input = true;
  if (region_res) {
    pending.input = user_data<WlRegion>(region_res)->region;
    pending.input.infinite = false;
  } else {
    pending.input = {};
    pending.input.infinite = true;
  }
}

void Surface::set_buffer_transform(wl_resource*, int32_t transform) {
  if (transform < 0 || transform > 7) {
    wl_resource_post_error(res, WL_SURFACE_ERROR_INVALID_TRANSFORM,
                           "invalid transform %d", transform);
    return;
  }
  pending.transform = transform;
}

void Surface::set_buffer_scale(wl_resource*, int32_t scale) {
  if (scale <= 0) {
    wl_resource_post_error(res, WL_SURFACE_ERROR_INVALID_SCALE,
                           "invalid scale %d", scale);
    return;
  }
  pending.scale = scale;
}

void Surface::offset(wl_resource*, int32_t dx, int32_t dy) {
  if (pending.has_buffer) {
    pending.dx = dx;
    pending.dy = dy;
  }
}

static const struct wl_surface_interface surface_impl = {
    destroy_request,
    REQ(Surface, attach),
    REQ(Surface, damage),
    REQ(Surface, frame),
    REQ(Surface, set_opaque_region),
    REQ(Surface, set_input_region),
    REQ(Surface, commit_request),
    REQ(Surface, set_buffer_transform),
    REQ(Surface, set_buffer_scale),
    REQ(Surface, damage_buffer),
    REQ(Surface, offset),
};

// ------------------------------------------------------------------ wl_region

static const struct wl_region_interface region_impl = {
    destroy_request, REQ(WlRegion, add), REQ(WlRegion, subtract),
};

// --------------------------------------------------------------- Subsurface

Subsurface::Subsurface(Server* server, Surface* s, Surface* p)
    : surface(s), server_(server), parent_(p) {
  s->subsurface = this;
  s->parent = p;
  s->sync = true;  // subsurfaces start in sync mode
  p->children.push_back(s);
}

Subsurface::~Subsurface() {
  surface->subsurface = nullptr;
  if (surface->parent) {
    auto& sib = surface->parent->children;
    sib.erase(std::remove(sib.begin(), sib.end(), surface), sib.end());
    surface->parent = nullptr;
    server_->mark_dirty();
  }
}

void Subsurface::set_position(wl_resource*, int32_t x, int32_t y) {
  surface->sub_x = x;
  surface->sub_y = y;
  server_->mark_dirty();
}

void Subsurface::place_above(wl_resource*, wl_resource* sibling_res) {
  Surface* sibling = user_data<Surface>(sibling_res);
  if (sibling->parent != parent_) return;
  auto& kids = parent_->children;
  kids.erase(std::remove(kids.begin(), kids.end(), surface), kids.end());
  auto it = std::find(kids.begin(), kids.end(), sibling);
  kids.insert(it + 1, surface);
  server_->mark_dirty();
}

void Subsurface::place_below(wl_resource*, wl_resource* sibling_res) {
  Surface* sibling = user_data<Surface>(sibling_res);
  if (sibling->parent != parent_) return;
  auto& kids = parent_->children;
  kids.erase(std::remove(kids.begin(), kids.end(), surface), kids.end());
  auto it = std::find(kids.begin(), kids.end(), sibling);
  kids.insert(it, surface);
  server_->mark_dirty();
}

void Subsurface::set_sync(wl_resource*) { surface->sync = true; }
void Subsurface::set_desync(wl_resource*) { surface->sync = false; }

static const struct wl_subsurface_interface subsurface_impl = {
    destroy_request,
    REQ(Subsurface, set_position),
    REQ(Subsurface, place_above),
    REQ(Subsurface, place_below),
    REQ(Subsurface, set_sync),
    REQ(Subsurface, set_desync),
};

// ------------------------------------------------------------ wl_compositor

static const struct wl_compositor_interface compositor_impl = {
    REQ(Compositor, create_surface), REQ(Compositor, create_region),
};

Compositor::Compositor(Server* s) : server(s) {
  global_.create(s->display, &wl_compositor_interface, 5,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   wl_resource* res = wl_resource_create(
                       c, &wl_compositor_interface, v, id);
                   wl_resource_set_implementation(res, &compositor_impl, this,
                                                  nullptr);
                 });
}

void Compositor::create_surface(wl_client* client, wl_resource* res,
                                uint32_t id) {
  auto* s = bind_resource(client, &wl_surface_interface,
                          wl_resource_get_version(res), id, new Surface(server),
                          &surface_impl);
  server->surfaces.push_back(s);
}

void Compositor::create_region(wl_client* client, wl_resource*,
                               uint32_t id) {
  bind_resource(client, &wl_region_interface, 1, id, new WlRegion(),
                &region_impl);
}

// ---------------------------------------------------------- wl_subcompositor

static const struct wl_subcompositor_interface subcompositor_impl = {
    destroy_request, REQ(Subcompositor, get_subsurface),
};

Subcompositor::Subcompositor(Server* s) : server(s) {
  global_.create(s->display, &wl_subcompositor_interface, 1,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   wl_resource* res = wl_resource_create(
                       c, &wl_subcompositor_interface, v, id);
                   wl_resource_set_implementation(res, &subcompositor_impl,
                                                  this, nullptr);
                 });
}

void Subcompositor::get_subsurface(wl_client* client, wl_resource* res,
                                   uint32_t id, wl_resource* surface_res,
                                   wl_resource* parent_res) {
  Surface* s = user_data<Surface>(surface_res);
  Surface* parent = user_data<Surface>(parent_res);
  if (s == parent) {
    wl_resource_post_error(res, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "surface cannot be its own parent");
    return;
  }
  if (s->subsurface) {
    wl_resource_post_error(res, WL_SUBCOMPOSITOR_ERROR_BAD_SURFACE,
                           "surface already has a parent");
    return;
  }
  bind_resource(client, &wl_subsurface_interface, wl_resource_get_version(res),
                id, new Subsurface(server, s, parent), &subsurface_impl);
}

}  // namespace oasis
