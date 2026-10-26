#include "xdg.h"

#include <algorithm>
#include "log.h"
#include "seat.h"
#include "server.h"
#include "surface.h"

namespace oasis {

// -------------------------------------------------------- interface tables

static const struct xdg_positioner_interface positioner_impl = {
    destroy_request,
    REQ(Positioner, set_size),
    REQ(Positioner, set_anchor_rect),
    REQ(Positioner, set_anchor),
    REQ(Positioner, set_gravity),
    REQ(Positioner, set_constraint),
    REQ(Positioner, set_offset),
    REQ(Positioner, set_reactive),
    REQ(Positioner, set_parent_size),
    REQ(Positioner, set_parent_configure),
};

static const struct xdg_surface_interface xdg_surface_impl = {
    destroy_request,
    REQ(XdgSurface, get_toplevel),
    REQ(XdgSurface, get_popup),
    REQ(XdgSurface, set_window_geometry),
    REQ(XdgSurface, ack_configure),
};

static const struct xdg_toplevel_interface toplevel_impl = {
    destroy_request,
    REQ(XdgToplevel, set_parent),
    REQ(XdgToplevel, set_title),
    REQ(XdgToplevel, set_app_id),
    REQ(XdgToplevel, show_window_menu),
    REQ(XdgToplevel, move),
    REQ(XdgToplevel, resize),
    REQ(XdgToplevel, set_max_size),
    REQ(XdgToplevel, set_min_size),
    REQ(XdgToplevel, set_maximized),
    REQ(XdgToplevel, unset_maximized),
    REQ(XdgToplevel, set_fullscreen),
    REQ(XdgToplevel, unset_fullscreen),
    REQ(XdgToplevel, set_minimized),
};

static const struct xdg_popup_interface popup_impl = {
    destroy_request,
    REQ(XdgPopup, grab),
    REQ(XdgPopup, reposition),
};

static const struct xdg_wm_base_interface wm_impl = {
    REQ(XdgShell, destroy),
    REQ(XdgShell, create_positioner),
    REQ(XdgShell, get_xdg_surface),
    REQ(XdgShell, pong),
};

// ------------------------------------------------------------- Positioner

void Positioner::resolve(int32_t parent_lx, int32_t parent_ly, int32_t out_w,
                         int32_t out_h, int32_t* lx, int32_t* ly) const {
  // anchor point inside anchor_rect, in parent-surface coords
  int32_t ax = anchor_rect.x, ay = anchor_rect.y;
  switch (anchor) {
    case XDG_POSITIONER_ANCHOR_TOP:
    case XDG_POSITIONER_ANCHOR_BOTTOM:
      ax += anchor_rect.w / 2;
      break;
    case XDG_POSITIONER_ANCHOR_RIGHT:
    case XDG_POSITIONER_ANCHOR_TOP_RIGHT:
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      ax += anchor_rect.w;
      break;
    default:
      break;
  }
  switch (anchor) {
    case XDG_POSITIONER_ANCHOR_LEFT:
    case XDG_POSITIONER_ANCHOR_RIGHT:
      ay += anchor_rect.h / 2;
      break;
    case XDG_POSITIONER_ANCHOR_BOTTOM:
    case XDG_POSITIONER_ANCHOR_BOTTOM_LEFT:
    case XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT:
      ay += anchor_rect.h;
      break;
    default:
      break;
  }

  // popup origin relative to anchor point, from gravity
  int32_t gx = -w, gy = -h;
  switch (gravity) {
    case XDG_POSITIONER_GRAVITY_TOP:
    case XDG_POSITIONER_GRAVITY_BOTTOM:
      gx = -w / 2;
      break;
    case XDG_POSITIONER_GRAVITY_RIGHT:
    case XDG_POSITIONER_GRAVITY_TOP_RIGHT:
    case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
      gx = 0;
      break;
    default:
      break;
  }
  switch (gravity) {
    case XDG_POSITIONER_GRAVITY_LEFT:
    case XDG_POSITIONER_GRAVITY_RIGHT:
      gy = -h / 2;
      break;
    case XDG_POSITIONER_GRAVITY_BOTTOM:
    case XDG_POSITIONER_GRAVITY_BOTTOM_LEFT:
    case XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT:
      gy = 0;
      break;
    default:
      break;
  }

  int32_t x = ax + gx + ox, y = ay + gy + oy;
  int32_t Lx = parent_lx + x, Ly = parent_ly + y;

  if ((constraint & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X) &&
      (Lx + w > out_w || Lx < 0)) {
    static const uint32_t flip_anchor[] = {
        XDG_POSITIONER_ANCHOR_NONE,         XDG_POSITIONER_ANCHOR_TOP,
        XDG_POSITIONER_ANCHOR_BOTTOM,       XDG_POSITIONER_ANCHOR_RIGHT,
        XDG_POSITIONER_ANCHOR_LEFT,         XDG_POSITIONER_ANCHOR_TOP_RIGHT,
        XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT, XDG_POSITIONER_ANCHOR_TOP_LEFT,
        XDG_POSITIONER_ANCHOR_BOTTOM_LEFT};
    static const uint32_t flip_gravity[] = {
        XDG_POSITIONER_GRAVITY_NONE,         XDG_POSITIONER_GRAVITY_TOP,
        XDG_POSITIONER_GRAVITY_BOTTOM,       XDG_POSITIONER_GRAVITY_RIGHT,
        XDG_POSITIONER_GRAVITY_LEFT,         XDG_POSITIONER_GRAVITY_TOP_RIGHT,
        XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT, XDG_POSITIONER_GRAVITY_TOP_LEFT,
        XDG_POSITIONER_GRAVITY_BOTTOM_LEFT};
    Positioner p = *this;
    if (anchor <= XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
      p.anchor = flip_anchor[anchor];
    if (gravity <= XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
      p.gravity = flip_gravity[gravity];
    p.constraint &= ~(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X |
                      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    p.resolve(parent_lx, parent_ly, out_w, out_h, lx, ly);
    return;
  }
  if ((constraint & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y) &&
      (Ly + h > out_h || Ly < 0)) {
    static const uint32_t flip_anchor[] = {
        XDG_POSITIONER_ANCHOR_NONE,         XDG_POSITIONER_ANCHOR_BOTTOM,
        XDG_POSITIONER_ANCHOR_TOP,          XDG_POSITIONER_ANCHOR_LEFT,
        XDG_POSITIONER_ANCHOR_RIGHT,        XDG_POSITIONER_ANCHOR_BOTTOM_LEFT,
        XDG_POSITIONER_ANCHOR_TOP_LEFT,     XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT,
        XDG_POSITIONER_ANCHOR_TOP_RIGHT};
    static const uint32_t flip_gravity[] = {
        XDG_POSITIONER_GRAVITY_NONE,         XDG_POSITIONER_GRAVITY_BOTTOM,
        XDG_POSITIONER_GRAVITY_TOP,          XDG_POSITIONER_GRAVITY_LEFT,
        XDG_POSITIONER_GRAVITY_RIGHT,        XDG_POSITIONER_GRAVITY_BOTTOM_LEFT,
        XDG_POSITIONER_GRAVITY_TOP_LEFT,     XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
        XDG_POSITIONER_GRAVITY_TOP_RIGHT};
    Positioner p = *this;
    if (anchor <= XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT)
      p.anchor = flip_anchor[anchor];
    if (gravity <= XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT)
      p.gravity = flip_gravity[gravity];
    p.constraint &= ~(XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X |
                      XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y);
    p.resolve(parent_lx, parent_ly, out_w, out_h, lx, ly);
    return;
  }
  if (constraint & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X)
    Lx = std::clamp(Lx, 0, std::max(0, out_w - w));
  if (constraint & XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y)
    Ly = std::clamp(Ly, 0, std::max(0, out_h - h));

  *lx = Lx;
  *ly = Ly;
}

// --------------------------------------------------------------- XdgSurface

XdgSurface::XdgSurface(Server* srv, Surface* s) : server(srv), surface(s) {
  s->xdg = this;
}

XdgSurface::~XdgSurface() {
  if (surface) surface->xdg = nullptr;
}

void XdgSurface::get_toplevel(wl_client* client, wl_resource*, uint32_t id) {
  if (toplevel || popup) {
    wl_resource_post_error(res, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                           "surface already has a role");
    return;
  }
  toplevel = bind_resource(client, &xdg_toplevel_interface,
                           wl_resource_get_version(res), id,
                           new XdgToplevel(server, this), &toplevel_impl);
}

void XdgSurface::get_popup(wl_client* client, wl_resource*, uint32_t id,
                           wl_resource* parent_res,
                           wl_resource* positioner_res) {
  if (toplevel || popup) {
    wl_resource_post_error(res, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                           "surface already has a role");
    return;
  }
  Surface* parent =
      parent_res ? user_data<Surface>(parent_res) : nullptr;
  if (!parent) {
    wl_resource_post_error(res, XDG_WM_BASE_ERROR_INVALID_POPUP_PARENT,
                           "popup needs a parent");
    return;
  }
  auto* pos = user_data<Positioner>(positioner_res);
  popup = bind_resource(client, &xdg_popup_interface,
                        wl_resource_get_version(res), id,
                        new XdgPopup(server, this, parent), &popup_impl);
  server->add_popup(popup);

  int32_t plx = 0, ply = 0;
  parent->layout_pos(&plx, &ply);
  Backend::Output* out = server->primary_output();
  int32_t ow = out ? out->width : 4096, oh = out ? out->height : 4096;
  pos->resolve(plx, ply, ow, oh, &popup->lx, &popup->ly);
}

void XdgSurface::committed() {
  if (!toplevel) return;
  if (surface->mapped()) {
    bool listed = std::find(server->toplevels.begin(),
                            server->toplevels.end(),
                            toplevel) != server->toplevels.end();
    if (!toplevel->configure_sent) {
      toplevel->configure_sent = true;
      toplevel->activated = true;
      toplevel->send_configure(0, 0, {XDG_TOPLEVEL_STATE_ACTIVATED});
    }
    if (!listed) {
      server->add_toplevel(toplevel);
      server->seat->set_keyboard_focus(surface);
    }
  } else if (std::find(server->toplevels.begin(), server->toplevels.end(),
                       toplevel) != server->toplevels.end()) {
    server->remove_toplevel(toplevel);
  }
}

// -------------------------------------------------------------- xdg_toplevel

XdgToplevel::XdgToplevel(Server* srv, XdgSurface* x) : server(srv), xdg(x) {}

XdgToplevel::~XdgToplevel() {
  server->remove_toplevel(this);
  if (server->seat->grab_toplevel == this) {
    server->seat->grab = Seat::Grab::None;
    server->seat->grab_toplevel = nullptr;
  }
  if (xdg) xdg->toplevel = nullptr;
}

Surface* XdgToplevel::surface() const { return xdg ? xdg->surface : nullptr; }

Rect XdgToplevel::bounds() const {
  Surface* s = surface();
  if (!s) return {x, y, 0, 0};
  return {x, y, s->width(), s->height()};
}

std::vector<uint32_t> XdgToplevel::states() const {
  std::vector<uint32_t> states;
  if (activated) states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
  if (maximized) states.push_back(XDG_TOPLEVEL_STATE_MAXIMIZED);
  if (fullscreen) states.push_back(XDG_TOPLEVEL_STATE_FULLSCREEN);
  return states;
}

void XdgToplevel::send_configure(int32_t w, int32_t h,
                                 const std::vector<uint32_t>& states) {
  WlArray arr;
  arr.add(states);
  xdg_toplevel_send_configure(res, w, h, arr.get());
  xdg->pending_serial = wl_display_next_serial(server->display);
  xdg_surface_send_configure(xdg->res, xdg->pending_serial);
}

void XdgToplevel::set_title(wl_resource*, const char* t) {
  title = t ? t : "";
  server->schedule_windows_notify();
}

void XdgToplevel::set_app_id(wl_resource*, const char* a) {
  app_id = a ? a : "";
  server->schedule_windows_notify();
}

void XdgToplevel::move(wl_resource*, wl_resource*, uint32_t) {
  server->seat->begin_move(this);
}

void XdgToplevel::resize(wl_resource*, wl_resource*, uint32_t,
                         uint32_t edges) {
  server->seat->begin_resize(this, edges);
}

void XdgToplevel::set_maximized(wl_resource*) {
  if (maximized) return;
  Backend::Output* out = server->primary_output();
  if (!out) return;
  saved_x = x;
  saved_y = y;
  x = out->x;
  y = out->y;
  maximized = true;
  send_configure(out->width, out->height, states());
}

void XdgToplevel::unset_maximized(wl_resource*) {
  if (!maximized) return;
  maximized = false;
  x = saved_x;
  y = saved_y;
  send_configure(0, 0, states());
}

void XdgToplevel::set_fullscreen(wl_resource*, wl_resource*) {
  if (fullscreen) return;
  Backend::Output* out = server->primary_output();
  if (!out) return;
  if (!maximized) {
    saved_x = x;
    saved_y = y;
  }
  x = out->x;
  y = out->y;
  fullscreen = true;
  send_configure(out->width, out->height, states());
}

void XdgToplevel::unset_fullscreen(wl_resource*) {
  if (!fullscreen) return;
  fullscreen = false;
  x = saved_x;
  y = saved_y;
  send_configure(0, 0, states());
}

// ----------------------------------------------------------------- xdg_popup

XdgPopup::XdgPopup(Server* srv, XdgSurface* x, Surface* p)
    : server(srv), xdg(x), surface(x->surface), parent(p) {}

XdgPopup::~XdgPopup() {
  res = nullptr;  // resource is already dying; never send on it
  server->remove_popup(this);
  server->seat->release_popups_from(this);
  if (xdg) xdg->popup = nullptr;
}

void XdgPopup::grab(wl_resource*, wl_resource*, uint32_t) {
  server->seat->popup_grab(this);
}

void XdgPopup::reposition(wl_resource*, wl_resource* positioner_res,
                          uint32_t) {
  auto* pos = user_data<Positioner>(positioner_res);
  int32_t plx = 0, ply = 0;
  parent->layout_pos(&plx, &ply);
  Backend::Output* out = server->primary_output();
  int32_t ow = out ? out->width : 4096, oh = out ? out->height : 4096;
  pos->resolve(plx, ply, ow, oh, &lx, &ly);
  xdg_popup_send_repositioned(res, 0);
  server->mark_dirty();
}

// ---------------------------------------------------------------- xdg_wm_base

XdgShell::XdgShell(Server* s) : server(s) {
  global_.create(s->display, &xdg_wm_base_interface, 3,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   wl_resource* res = wl_resource_create(
                       c, &xdg_wm_base_interface, v, id);
                   wl_resource_set_implementation(res, &wm_impl, this,
                                                  nullptr);
                 });
}

void XdgShell::create_positioner(wl_client* client, wl_resource* res,
                                 uint32_t id) {
  bind_resource(client, &xdg_positioner_interface,
                wl_resource_get_version(res), id, new Positioner(),
                &positioner_impl);
}

void XdgShell::get_xdg_surface(wl_client* client, wl_resource* res,
                               uint32_t id, wl_resource* surface_res) {
  Surface* surface = user_data<Surface>(surface_res);
  if (surface->xdg) {
    wl_resource_post_error(res, XDG_WM_BASE_ERROR_ROLE,
                           "surface already has a role");
    return;
  }
  bind_resource(client, &xdg_surface_interface, wl_resource_get_version(res),
                id, new XdgSurface(server, surface), &xdg_surface_impl);
}

}  // namespace oasis
