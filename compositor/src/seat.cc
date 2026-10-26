#include "seat.h"

#include <linux/input-event-codes.h>
#include <algorithm>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include "flutter.h"
#include "log.h"
#include "protocols.h"
#include "server.h"
#include "surface.h"
#include "util.h"
#include "xdg.h"

namespace oasis {

// ------------------------------------------------------------------ wl_seat

static const struct wl_pointer_interface pointer_impl = {
    REQ(Seat, set_cursor), destroy_request,
};

static const struct wl_seat_interface seat_impl = {
    REQ(Seat, get_pointer), REQ(Seat, get_keyboard), REQ(Seat, get_touch),
};

Seat::Seat(Server* s) : server(s) {
  xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  set_keymap("");
}

Seat::~Seat() {
  if (kbd_state) xkb_state_unref(kbd_state);
  if (keymap) xkb_keymap_unref(keymap);
  if (xkb_ctx) xkb_context_unref(xkb_ctx);
}

bool Seat::init() {
  return global_.create(server->display, &wl_seat_interface, 5,
                        [this](wl_client* c, uint32_t v, uint32_t id) {
                          bind(c, v, id);
                        });
}

void Seat::bind(wl_client* client, uint32_t version, uint32_t id) {
  version = std::min<uint32_t>(version, 5);
  wl_resource* res =
      wl_resource_create(client, &wl_seat_interface, version, id);
  wl_resource_set_implementation(res, &seat_impl, this, nullptr);
  uint32_t caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
  wl_seat_send_capabilities(res, caps);
  if (version >= WL_SEAT_NAME_SINCE_VERSION)
    wl_seat_send_name(res, "oasis-seat0");
}

void Seat::pointer_destroyed(wl_resource* res) {
  auto* seat = user_data<Seat>(res);
  auto& v = seat->pointers;
  v.erase(std::remove(v.begin(), v.end(), res), v.end());
}

void Seat::keyboard_destroyed(wl_resource* res) {
  auto* seat = user_data<Seat>(res);
  auto& v = seat->keyboards;
  v.erase(std::remove(v.begin(), v.end(), res), v.end());
}

void Seat::set_cursor(wl_resource* res, uint32_t serial,
                      wl_resource* surface_res, int32_t hx, int32_t hy) {
  Surface* s = surface_res ? user_data<Surface>(surface_res) : nullptr;
  // Only honour cursor changes from the client that currently holds pointer
  // focus, matching how other compositors arbitrate the cursor.
  if (pointer_focus &&
      wl_resource_get_client(pointer_focus->res) !=
          wl_resource_get_client(res))
    return;
  server->cursor_surface = s;
  server->cursor_hx = hx;
  server->cursor_hy = hy;
  server->mark_dirty();
}

void Seat::get_pointer(wl_client* client, wl_resource* res, uint32_t id) {
  wl_resource* pres = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(res), id);
  wl_resource_set_implementation(pres, &pointer_impl, this,
                                 &Seat::pointer_destroyed);
  pointers.push_back(pres);
  if (pointer_focus && wl_resource_get_client(pointer_focus->res) == client) {
    wl_pointer_send_enter(pres, wl_display_next_serial(server->display),
                          pointer_focus->res, wl_fixed_from_double(focus_sx),
                          wl_fixed_from_double(focus_sy));
  }
}

void Seat::get_keyboard(wl_client* client, wl_resource* res, uint32_t id) {
  wl_resource* kres = wl_resource_create(
      client, &wl_keyboard_interface, wl_resource_get_version(res), id);
  wl_resource_set_implementation(kres, nullptr, this,
                                 &Seat::keyboard_destroyed);
  keyboards.push_back(kres);
  send_keymap(kres);
  if (wl_resource_get_version(kres) >= WL_KEYBOARD_REPEAT_INFO_SINCE_VERSION)
    wl_keyboard_send_repeat_info(kres, 0, 0);
  if (keyboard_focus && wl_resource_get_client(keyboard_focus->res) == client) {
    wl_array keys;
    wl_array_init(&keys);
    wl_keyboard_send_enter(kres, wl_display_next_serial(server->display),
                           keyboard_focus->res, &keys);
    wl_keyboard_send_modifiers(kres, wl_display_next_serial(server->display),
                               mods_dep, mods_lat, mods_lock, group);
  }
}

void Seat::get_touch(wl_client* client, wl_resource* res, uint32_t id) {
  wl_resource* tres = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(res), id);
  wl_resource_set_implementation(tres, nullptr, nullptr, nullptr);
}

// ------------------------------------------------------------------ keymap

void Seat::set_keymap(const std::string& str) {
  xkb_keymap* km = nullptr;
  if (!str.empty()) {
    km = xkb_keymap_new_from_string(xkb_ctx, str.c_str(),
                                    XKB_KEYMAP_FORMAT_TEXT_V1,
                                    XKB_KEYMAP_COMPILE_NO_FLAGS);
  }
  if (!km) {
    km = xkb_keymap_new_from_names(xkb_ctx, nullptr,
                                   XKB_KEYMAP_COMPILE_NO_FLAGS);
  }
  if (!km) {
    LOGE("failed to build keymap");
    return;
  }
  char* s = xkb_keymap_get_as_string(km, XKB_KEYMAP_FORMAT_TEXT_V1);
  keymap_str = s ? s : "";
  free(s);
  if (keymap) xkb_keymap_unref(keymap);
  keymap = km;
  if (kbd_state) xkb_state_unref(kbd_state);
  kbd_state = xkb_state_new(keymap);
  for (wl_resource* kbd : keyboards) send_keymap(kbd);
}

void Seat::send_keymap(wl_resource* kbd) {
  Fd fd(memfd_create_cloexec("oasis-keymap"));
  if (!fd) return;
  ftruncate(fd.get(), keymap_str.size() + 1);
  write_all(fd.get(), keymap_str.data(), keymap_str.size() + 1);
  lseek(fd.get(), 0, SEEK_SET);
  wl_keyboard_send_keymap(kbd, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd.get(),
                          keymap_str.size() + 1);
}

void Seat::send_modifiers() {
  if (!keyboard_focus) return;
  wl_client* fc = wl_resource_get_client(keyboard_focus->res);
  uint32_t serial = wl_display_next_serial(server->display);
  for (wl_resource* kbd : keyboards)
    if (wl_resource_get_client(kbd) == fc)
      wl_keyboard_send_modifiers(kbd, serial, mods_dep, mods_lat, mods_lock,
                                 group);
}

// ------------------------------------------------------------------- focus

void Seat::set_pointer_focus(Surface* s, double sx, double sy) {
  if (s == pointer_focus) {
    focus_sx = sx;
    focus_sy = sy;
    return;
  }
  Surface* old = pointer_focus;
  pointer_focus = s;
  focus_sx = sx;
  focus_sy = sy;
  uint32_t serial = wl_display_next_serial(server->display);
  if (old) {
    wl_client* oc = wl_resource_get_client(old->res);
    for (wl_resource* p : pointers)
      if (wl_resource_get_client(p) == oc)
        wl_pointer_send_leave(p, serial, old->res);
  }
  if (s) {
    wl_client* nc = wl_resource_get_client(s->res);
    for (wl_resource* p : pointers)
      if (wl_resource_get_client(p) == nc)
        wl_pointer_send_enter(p, serial, s->res, wl_fixed_from_double(sx),
                              wl_fixed_from_double(sy));
  }
}

void Seat::set_keyboard_focus(Surface* s) {
  if (s == keyboard_focus) return;
  Surface* old = keyboard_focus;
  keyboard_focus = s;
  uint32_t serial = wl_display_next_serial(server->display);
  if (old) {
    wl_client* oc = wl_resource_get_client(old->res);
    for (wl_resource* kbd : keyboards)
      if (wl_resource_get_client(kbd) == oc)
        wl_keyboard_send_leave(kbd, serial, old->res);
  }
  if (s) {
    wl_client* nc = wl_resource_get_client(s->res);
    wl_array keys;
    wl_array_init(&keys);
    for (wl_resource* kbd : keyboards)
      if (wl_resource_get_client(kbd) == nc)
        wl_keyboard_send_enter(kbd, serial, s->res, &keys);
    wl_array_release(&keys);
    send_modifiers();
  }
  server->data_manager->notify_selection();
}

// ------------------------------------------------------------------ events

void Seat::motion(double x, double y, uint32_t t) {
  server->cursor_x = x;
  server->cursor_y = y;
  server->mark_dirty();

  if (grab == Grab::Move && grab_toplevel) {
    grab_toplevel->x = (int32_t)(x - grab_x);
    grab_toplevel->y = (int32_t)(y - grab_y);
    return;
  }
  if (grab == Grab::Resize && grab_toplevel) {
    XdgToplevel* tl = grab_toplevel;
    int32_t nx = tl->x, ny = tl->y;
    int32_t w = tl->surface()->width(), h = tl->surface()->height();
    double dx = x - grab_x, dy = y - grab_y;
    if (grab_edges & XDG_TOPLEVEL_RESIZE_EDGE_LEFT) {
      nx = (int32_t)x;
      w -= (int32_t)dx;
    }
    if (grab_edges & XDG_TOPLEVEL_RESIZE_EDGE_RIGHT) w += (int32_t)dx;
    if (grab_edges & XDG_TOPLEVEL_RESIZE_EDGE_TOP) {
      ny = (int32_t)y;
      h -= (int32_t)dy;
    }
    if (grab_edges & XDG_TOPLEVEL_RESIZE_EDGE_BOTTOM) h += (int32_t)dy;
    if (w < 64) w = 64;
    if (h < 48) h = 48;
    tl->x = nx;
    tl->y = ny;
    auto states = tl->states();
    states.push_back(XDG_TOPLEVEL_STATE_RESIZING);
    tl->send_configure(w, h, states);
    return;
  }

  // popup grab: events confined to the grabbing popup tree
  Surface* s = nullptr;
  double sx = 0, sy = 0;
  if (!popup_stack.empty()) {
    XdgPopup* top = popup_stack.back();
    if (top->surface->hit(x - top->lx, y - top->ly, &sx, &sy)) {
      s = top->surface;
    }
  } else {
    server->surface_at(x, y, &s, &sx, &sy);
  }

  set_pointer_focus(s, sx, sy);
  if (s) {
    wl_client* c = wl_resource_get_client(s->res);
    for (wl_resource* p : pointers)
      if (wl_resource_get_client(p) == c)
        wl_pointer_send_motion(p, t, wl_fixed_from_double(sx),
                               wl_fixed_from_double(sy));
  }
  // shell pointer: over shell areas the events go to Flutter
  bool over_client = s != nullptr;
  if (!over_client && server->flutter && server->flutter->running()) {
    if (!flutter_added) {
      server->flutter->pointer(4 /*kAdd*/, x, y, flutter_buttons, now_usec());
      flutter_added = true;
    }
    server->flutter->pointer(flutter_buttons ? 3 /*kMove*/ : 6 /*kHover*/, x,
                             y, flutter_buttons, now_usec());
  } else if (over_client && flutter_added) {
    server->flutter->pointer(5 /*kRemove*/, x, y, 0, now_usec());
    flutter_added = false;
  }
}

void Seat::button(uint32_t button, bool pressed, uint32_t t) {
  int64_t mask = 0;
  if (button == BTN_LEFT) mask = 1;
  else if (button == BTN_RIGHT) mask = 2;
  else if (button == BTN_MIDDLE) mask = 4;
  else if (button < BTN_LEFT + 8) mask = 1LL << (button - BTN_LEFT);

  if (grab != Grab::None && !pressed) {
    grab = Grab::None;
    grab_toplevel = nullptr;
    return;
  }

  Surface* s = pointer_focus;
  if (pressed && s && !popup_stack.empty()) {
    // a click outside the grabbing popup tree dismisses it
    bool inside = false;
    for (XdgPopup* p : popup_stack)
      if (p->surface == s) inside = true;
    if (!inside) release_popups_from(nullptr);
  }

  if (pressed && s) {
    // raise/focus the toplevel that owns this surface
    Surface* root = s;
    while (root->parent) root = root->parent;
    for (XdgToplevel* tl : server->toplevels)
      if (tl->surface() == root) {
        server->raise(tl);
        set_keyboard_focus(root);
      }
  }
  if (pressed && !s) set_keyboard_focus(nullptr);

  if (s) {
    wl_client* c = wl_resource_get_client(s->res);
    uint32_t serial = wl_display_next_serial(server->display);
    for (wl_resource* p : pointers)
      if (wl_resource_get_client(p) == c)
        wl_pointer_send_button(p, serial, t, button,
                               pressed ? WL_POINTER_BUTTON_STATE_PRESSED
                                       : WL_POINTER_BUTTON_STATE_RELEASED);
  }

  bool was_down = flutter_buttons != 0;
  if (pressed)
    flutter_buttons |= mask;
  else
    flutter_buttons &= ~mask;
  if (flutter_added) {
    int phase;
    if (pressed)
      phase = was_down ? 3 /*kMove*/ : 2 /*kDown*/;
    else
      phase = flutter_buttons ? 3 /*kMove*/ : 1 /*kUp*/;
    server->flutter->pointer(phase, server->cursor_x, server->cursor_y,
                             flutter_buttons, now_usec());
  }
}

void Seat::axis(double dx, double dy, uint32_t t) {
  Surface* s = pointer_focus;
  if (s && popup_stack.empty()) {
    wl_client* c = wl_resource_get_client(s->res);
    for (wl_resource* p : pointers) {
      if (wl_resource_get_client(p) != c) continue;
      if (dy != 0)
        wl_pointer_send_axis(p, t, WL_POINTER_AXIS_VERTICAL_SCROLL,
                             wl_fixed_from_double(dy));
      if (dx != 0)
        wl_pointer_send_axis(p, t, WL_POINTER_AXIS_HORIZONTAL_SCROLL,
                             wl_fixed_from_double(dx));
      if (wl_resource_get_version(p) >= WL_POINTER_FRAME_SINCE_VERSION)
        wl_pointer_send_frame(p);
    }
    return;
  }
  if (flutter_added)
    server->flutter->pointer(6 /*kHover*/, server->cursor_x, server->cursor_y,
                             flutter_buttons, now_usec(), dx, dy);
}

void Seat::key(uint32_t evdev, bool pressed, uint32_t t) {
  xkb_keycode_t kc = evdev + 8;
  xkb_state_update_key(kbd_state, kc,
                       pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
  uint32_t serial = wl_display_next_serial(server->display);
  if (keyboard_focus) {
    wl_client* c = wl_resource_get_client(keyboard_focus->res);
    for (wl_resource* kbd : keyboards)
      if (wl_resource_get_client(kbd) == c)
        wl_keyboard_send_key(kbd, serial, t, evdev,
                             pressed ? WL_KEYBOARD_KEY_STATE_PRESSED
                                     : WL_KEYBOARD_KEY_STATE_RELEASED);
  } else if (server->flutter && server->flutter->running()) {
    server->flutter->key(evdev, pressed, kbd_state, kc, now_usec());
  }
}

void Seat::modifiers(uint32_t dep, uint32_t lat, uint32_t lock, uint32_t grp) {
  mods_dep = dep;
  mods_lat = lat;
  mods_lock = lock;
  group = grp;
  send_modifiers();
}

// ------------------------------------------------------------------- grabs

void Seat::begin_move(XdgToplevel* t) {
  grab = Grab::Move;
  grab_toplevel = t;
  grab_x = server->cursor_x - t->x;
  grab_y = server->cursor_y - t->y;
}

void Seat::begin_resize(XdgToplevel* t, uint32_t edges) {
  grab = Grab::Resize;
  grab_toplevel = t;
  grab_edges = edges;
  grab_x = server->cursor_x;
  grab_y = server->cursor_y;
}

void Seat::popup_grab(XdgPopup* p) {
  p->grabbed = true;
  popup_stack.push_back(p);
}

void Seat::release_popups_from(XdgPopup* p) {
  bool found = (p == nullptr);
  for (size_t i = 0; i < popup_stack.size();) {
    if (popup_stack[i] == p) found = true;
    if (found) {
      XdgPopup* pop = popup_stack[i];
      popup_stack.erase(popup_stack.begin() + i);
      if (pop->res) xdg_popup_send_popup_done(pop->res);
    } else {
      i++;
    }
  }
}

}  // namespace oasis
