#include "xdg.h"

#include <algorithm>
#include "log.h"
#include "seat.h"
#include "server.h"
#include "surface.h"

namespace oasis {

extern const struct xdg_toplevel_interface toplevel_impl;
extern const struct xdg_popup_interface popup_impl;

// ------------------------------------------------------------- Positioner

static void positioner_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}
static void positioner_set_size(wl_client*, wl_resource* res, int32_t w,
                                int32_t h) {
  auto* p = static_cast<Positioner*>(wl_resource_get_user_data(res));
  p->w = w;
  p->h = h;
}
static void positioner_set_anchor_rect(wl_client*, wl_resource* res,
                                       int32_t x, int32_t y, int32_t w,
                                       int32_t h) {
  auto* p = static_cast<Positioner*>(wl_resource_get_user_data(res));
  p->anchor_rect = {x, y, w, h};
}
static void positioner_set_anchor(wl_client*, wl_resource* res,
                                  uint32_t anchor) {
  auto* p = static_cast<Positioner*>(wl_resource_get_user_data(res));
  p->anchor = anchor;
}
static void positioner_set_gravity(wl_client*, wl_resource* res,
                                   uint32_t gravity) {
  auto* p = static_cast<Positioner*>(wl_resource_get_user_data(res));
  p->gravity = gravity;
}
static void positioner_set_constraint(wl_client*, wl_resource* res,
                                      uint32_t c) {
  auto* p = static_cast<Positioner*>(wl_resource_get_user_data(res));
  p->constraint = c;
}
static void positioner_set_offset(wl_client*, wl_resource* res, int32_t x,
                                  int32_t y) {
  auto* p = static_cast<Positioner*>(wl_resource_get_user_data(res));
  p->ox = x;
  p->oy = y;
}
static void positioner_set_reactive(wl_client*, wl_resource*) {}
static void positioner_set_parent_size(wl_client*, wl_resource*, int32_t,
                                       int32_t) {}
static void positioner_set_parent_configure(wl_client*, wl_resource*,
                                            uint32_t) {}

static const struct xdg_positioner_interface positioner_impl = {
    positioner_destroy,      positioner_set_size,
    positioner_set_anchor_rect, positioner_set_anchor,
    positioner_set_gravity,  positioner_set_constraint,
    positioner_set_offset,   positioner_set_reactive,
    positioner_set_parent_size, positioner_set_parent_configure,
};

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

Surface* XdgToplevel::surface() const { return xdg ? xdg->surface : nullptr; }

Rect XdgToplevel::bounds() const {
  Surface* s = surface();
  if (!s) return {x, y, 0, 0};
  return {x, y, s->width(), s->height()};
}

void XdgToplevel::send_configure(int32_t w, int32_t h,
                                 const std::vector<uint32_t>& states) {
  wl_array arr;
  wl_array_init(&arr);
  for (uint32_t st : states) {
    uint32_t* p = static_cast<uint32_t*>(wl_array_add(&arr, sizeof(st)));
    *p = st;
  }
  xdg_toplevel_send_configure(res, w, h, &arr);
  wl_array_release(&arr);
  xdg->pending_serial = wl_display_next_serial(server->display);
  xdg_surface_send_configure(xdg->res, xdg->pending_serial);
}

static void xdg_surface_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void xdg_surface_get_toplevel(wl_client* client, wl_resource* res,
                                   uint32_t id) {
  auto* xs = static_cast<XdgSurface*>(wl_resource_get_user_data(res));
  if (xs->toplevel || xs->popup) {
    wl_resource_post_error(res, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                           "surface already has a role");
    return;
  }
  wl_resource* tres = wl_resource_create(client, &xdg_toplevel_interface,
                                         wl_resource_get_version(res), id);
  auto* tl = new XdgToplevel();
  tl->server = xs->server;
  tl->res = tres;
  tl->xdg = xs;
  xs->toplevel = tl;
  wl_resource_set_implementation(tres, &toplevel_impl, tl, [](wl_resource* r) {
    auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(r));
    tl->server->remove_toplevel(tl);
    if (tl->server->seat->grab_toplevel == tl) {
      tl->server->seat->grab = Seat::Grab::None;
      tl->server->seat->grab_toplevel = nullptr;
    }
    if (tl->xdg) tl->xdg->toplevel = nullptr;
    delete tl;
  });
}

static void xdg_surface_get_popup(wl_client* client, wl_resource* res,
                                  uint32_t id, wl_resource* parent_res,
                                  wl_resource* positioner_res) {
  auto* xs = static_cast<XdgSurface*>(wl_resource_get_user_data(res));
  if (xs->toplevel || xs->popup) {
    wl_resource_post_error(res, XDG_SURFACE_ERROR_ALREADY_CONSTRUCTED,
                           "surface already has a role");
    return;
  }
  Surface* parent = parent_res
      ? static_cast<Surface*>(wl_resource_get_user_data(parent_res))
      : nullptr;
  if (!parent) {
    wl_resource_post_error(res, XDG_WM_BASE_ERROR_INVALID_POPUP_PARENT,
                           "popup needs a parent");
    return;
  }
  auto* pos = static_cast<Positioner*>(
      wl_resource_get_user_data(positioner_res));
  wl_resource* pres = wl_resource_create(client, &xdg_popup_interface,
                                         wl_resource_get_version(res), id);
  auto* pop = new XdgPopup();
  pop->server = xs->server;
  pop->res = pres;
  pop->surface = xs->surface;
  pop->parent = parent;
  xs->popup = pop;
  xs->surface->is_popup = true;
  server_popup_add(xs->server, pop);

  int32_t plx = 0, ply = 0;
  surface_layout_pos(parent, &plx, &ply);
  Backend::Output* out = xs->server->primary_output();
  int32_t ow = out ? out->width : 4096, oh = out ? out->height : 4096;
  pos->resolve(plx, ply, ow, oh, &pop->lx, &pop->ly);

  wl_resource_set_implementation(pres, &popup_impl, pop, [](wl_resource* r) {
    auto* pop = static_cast<XdgPopup*>(wl_resource_get_user_data(r));
    pop->res = nullptr;  // resource is already dying; never send on it
    server_popup_remove(pop->server, pop);
    pop->server->seat->release_popups_from(pop);
    if (pop->surface && pop->surface->xdg)
      pop->surface->xdg->popup = nullptr;
    delete pop;
  });
}

static void xdg_surface_set_window_geometry(wl_client*, wl_resource* res,
                                            int32_t x, int32_t y, int32_t w,
                                            int32_t h) {
  auto* xs = static_cast<XdgSurface*>(wl_resource_get_user_data(res));
  xs->geometry = {x, y, w, h};
  xs->has_geometry = true;
}

static void xdg_surface_ack_configure(wl_client*, wl_resource* res,
                                      uint32_t serial) {
  auto* xs = static_cast<XdgSurface*>(wl_resource_get_user_data(res));
  if (serial == xs->pending_serial) xs->pending_serial = 0;
}

static const struct xdg_surface_interface xdg_surface_impl = {
    xdg_surface_destroy,     xdg_surface_get_toplevel,
    xdg_surface_get_popup,   xdg_surface_set_window_geometry,
    xdg_surface_ack_configure,
};

// -------------------------------------------------------------- xdg_toplevel

static void toplevel_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}
static void toplevel_set_parent(wl_client*, wl_resource*, wl_resource*) {}
static void toplevel_set_title(wl_client*, wl_resource* res,
                               const char* title) {
  auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(res));
  tl->title = title ? title : "";
  tl->server->schedule_windows_notify();
}
static void toplevel_set_app_id(wl_client*, wl_resource* res,
                                const char* app_id) {
  auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(res));
  tl->app_id = app_id ? app_id : "";
  tl->server->schedule_windows_notify();
}
static void toplevel_show_window_menu(wl_client*, wl_resource*, wl_resource*,
                                      uint32_t, int32_t, int32_t) {}
static void toplevel_move(wl_client*, wl_resource* res, wl_resource*,
                          uint32_t) {
  auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(res));
  tl->server->seat->begin_move(tl);
}
static void toplevel_resize(wl_client*, wl_resource* res, wl_resource*,
                            uint32_t, uint32_t edges) {
  auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(res));
  tl->server->seat->begin_resize(tl, edges);
}
static void toplevel_set_max_size(wl_client*, wl_resource*, int32_t, int32_t) {}
static void toplevel_set_min_size(wl_client*, wl_resource*, int32_t, int32_t) {}
static void toplevel_set_maximized(wl_client*, wl_resource* res) {
  auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(res));
  if (!tl->maximized) {
    Backend::Output* out = tl->server->primary_output();
    if (out) {
      tl->saved_x = tl->x;
      tl->saved_y = tl->y;
      tl->x = out->x;
      tl->y = out->y;
      tl->maximized = true;
      std::vector<uint32_t> states = {XDG_TOPLEVEL_STATE_MAXIMIZED};
      if (tl->activated) states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
      tl->send_configure(out->width, out->height, states);
    }
  }
}
static void toplevel_unset_maximized(wl_client*, wl_resource* res) {
  auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(res));
  if (tl->maximized) {
    tl->maximized = false;
    tl->x = tl->saved_x;
    tl->y = tl->saved_y;
    std::vector<uint32_t> states;
    if (tl->activated) states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
    tl->send_configure(0, 0, states);
  }
}
static void toplevel_set_fullscreen(wl_client*, wl_resource* res,
                                    wl_resource*) {
  auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(res));
  if (!tl->fullscreen) {
    Backend::Output* out = tl->server->primary_output();
    if (out) {
      if (!tl->maximized) {
        tl->saved_x = tl->x;
        tl->saved_y = tl->y;
      }
      tl->x = out->x;
      tl->y = out->y;
      tl->fullscreen = true;
      std::vector<uint32_t> states = {XDG_TOPLEVEL_STATE_FULLSCREEN};
      if (tl->activated) states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
      tl->send_configure(out->width, out->height, states);
    }
  }
}
static void toplevel_unset_fullscreen(wl_client*, wl_resource* res) {
  auto* tl = static_cast<XdgToplevel*>(wl_resource_get_user_data(res));
  if (tl->fullscreen) {
    tl->fullscreen = false;
    tl->x = tl->saved_x;
    tl->y = tl->saved_y;
    std::vector<uint32_t> states;
    if (tl->activated) states.push_back(XDG_TOPLEVEL_STATE_ACTIVATED);
    tl->send_configure(0, 0, states);
  }
}
static void toplevel_set_minimized(wl_client*, wl_resource*) {}

const struct xdg_toplevel_interface toplevel_impl = {
    toplevel_destroy,        toplevel_set_parent,   toplevel_set_title,
    toplevel_set_app_id,     toplevel_show_window_menu, toplevel_move,
    toplevel_resize,         toplevel_set_max_size, toplevel_set_min_size,
    toplevel_set_maximized,  toplevel_unset_maximized,
    toplevel_set_fullscreen, toplevel_unset_fullscreen,
    toplevel_set_minimized,
};

// ----------------------------------------------------------------- xdg_popup

static void popup_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}
static void popup_grab(wl_client*, wl_resource* res, wl_resource*,
                       uint32_t) {
  auto* pop = static_cast<XdgPopup*>(wl_resource_get_user_data(res));
  pop->server->seat->popup_grab(pop);
}
static void popup_reposition(wl_client*, wl_resource* res,
                             wl_resource* positioner_res, uint32_t) {
  auto* pop = static_cast<XdgPopup*>(wl_resource_get_user_data(res));
  auto* pos = static_cast<Positioner*>(
      wl_resource_get_user_data(positioner_res));
  int32_t plx = 0, ply = 0;
  surface_layout_pos(pop->parent, &plx, &ply);
  Backend::Output* out = pop->server->primary_output();
  int32_t ow = out ? out->width : 4096, oh = out ? out->height : 4096;
  pos->resolve(plx, ply, ow, oh, &pop->lx, &pop->ly);
  xdg_popup_send_repositioned(pop->res, 0);
  pop->server->mark_dirty();
}

const struct xdg_popup_interface popup_impl = {
    popup_destroy, popup_grab, popup_reposition,
};

// ---------------------------------------------------------------- xdg_wm_base

static void wm_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void wm_create_positioner(wl_client* client, wl_resource* res,
                                 uint32_t id) {
  wl_resource* pres =
      wl_resource_create(client, &xdg_positioner_interface,
                         wl_resource_get_version(res), id);
  wl_resource_set_implementation(pres, &positioner_impl, new Positioner(),
                                 [](wl_resource* r) {
                                   delete static_cast<Positioner*>(
                                       wl_resource_get_user_data(r));
                                 });
}

static void wm_get_xdg_surface(wl_client* client, wl_resource* res,
                               uint32_t id, wl_resource* surface_res) {
  auto* server = static_cast<Server*>(wl_resource_get_user_data(res));
  auto* surface =
      static_cast<Surface*>(wl_resource_get_user_data(surface_res));
  if (surface->xdg) {
    wl_resource_post_error(res, XDG_WM_BASE_ERROR_ROLE,
                           "surface already has a role");
    return;
  }
  wl_resource* xres = wl_resource_create(client, &xdg_surface_interface,
                                         wl_resource_get_version(res), id);
  auto* xs = new XdgSurface();
  xs->server = server;
  xs->res = xres;
  xs->surface = surface;
  surface->xdg = xs;
  wl_resource_set_implementation(xres, &xdg_surface_impl, xs,
                                 [](wl_resource* r) {
                                   auto* xs = static_cast<XdgSurface*>(
                                       wl_resource_get_user_data(r));
                                   if (xs->surface) xs->surface->xdg = nullptr;
                                   delete xs;
                                 });
}

static void wm_pong(wl_client*, wl_resource*, uint32_t) {}

static const struct xdg_wm_base_interface wm_impl = {
    wm_destroy, wm_create_positioner, wm_get_xdg_surface, wm_pong,
};

static void wm_bind(wl_client* client, void* data, uint32_t version,
                    uint32_t id) {
  wl_resource* res =
      wl_resource_create(client, &xdg_wm_base_interface, version, id);
  wl_resource_set_implementation(res, &wm_impl, data, nullptr);
}

void xdg_global_init(Server* server) {
  wl_global_create(server->display, &xdg_wm_base_interface, 3, server,
                   wm_bind);
}

// ------------------------------------------------------- map/unmap handling

void xdg_surface_committed(Surface* s) {
  XdgSurface* xs = s->xdg;
  if (!xs) return;
  Server* server = xs->server;

  if (xs->toplevel) {
    XdgToplevel* tl = xs->toplevel;
    if (s->mapped()) {
      bool listed =
          std::find(server->toplevels.begin(), server->toplevels.end(), tl) !=
          server->toplevels.end();
      if (!tl->configure_sent) {
        tl->configure_sent = true;
        std::vector<uint32_t> states = {XDG_TOPLEVEL_STATE_ACTIVATED};
        tl->activated = true;
        tl->send_configure(0, 0, states);
      }
      if (!listed) {
        server->add_toplevel(tl);
        server->seat->set_keyboard_focus(s);
      }
    } else if (std::find(server->toplevels.begin(), server->toplevels.end(),
                         tl) != server->toplevels.end()) {
      server->remove_toplevel(tl);
    }
  }
}

// ------------------------------------------------------ server popup hooks

void server_popup_add(Server* server, XdgPopup* pop) {
  server->popups.push_back(pop);
}
void server_popup_remove(Server* server, XdgPopup* pop) {
  auto& v = server->popups;
  v.erase(std::remove(v.begin(), v.end(), pop), v.end());
}

void surface_layout_pos(Surface* s, int32_t* lx, int32_t* ly) {
  int32_t x = 0, y = 0;
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

}  // namespace oasis
