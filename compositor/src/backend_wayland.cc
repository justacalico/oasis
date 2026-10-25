// Nested backend: the compositor runs as an xdg toplevel client of a host
// Wayland compositor. Input and frame callbacks come from the host seat.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <unistd.h>
#include "backend.h"
#include "log.h"
#include "util.h"
#include "xdg-shell-client.h"

namespace oasis {

class WaylandBackend : public Backend {
 public:
  bool init(wl_event_loop* loop, Callbacks cb) override;
  EGLDisplay egl_display() const override { return edpy_; }
  EGLContext egl_context() const override { return ctx_; }
  EGLConfig egl_config() const override { return econf_; }
  EGLContext new_context() override;
  const std::vector<Output*>& outputs() const override { return outs_; }
  void begin_frame(Output* o) override;
  void end_frame(Output* o) override;
  void schedule_frame(Output* o) override;

  // host event handlers
  void registry_add(uint32_t name, const char* iface, uint32_t version);
  void toplevel_configure(int32_t w, int32_t h, wl_array* states);
  void surface_configure(uint32_t serial);
  void frame_done(uint32_t t);
  void pointer_enter(uint32_t serial, wl_surface* s, wl_fixed_t x,
                     wl_fixed_t y);
  void pointer_leave(uint32_t serial, wl_surface* s);
  void pointer_motion(uint32_t t, wl_fixed_t x, wl_fixed_t y);
  void pointer_button(uint32_t serial, uint32_t t, uint32_t button,
                      uint32_t state);
  void pointer_axis(uint32_t t, uint32_t axis, wl_fixed_t value);
  void pointer_frame();
  void keyboard_keymap(uint32_t format, int32_t fd, uint32_t size);
  void keyboard_enter(uint32_t serial, wl_surface* s, wl_array* keys);
  void keyboard_leave(uint32_t serial, wl_surface* s);
  void keyboard_key(uint32_t serial, uint32_t t, uint32_t key,
                    uint32_t state);
  void keyboard_modifiers(uint32_t serial, uint32_t dep, uint32_t lat,
                          uint32_t lock, uint32_t grp);

  Callbacks cb_;
  wl_event_loop* loop_ = nullptr;
  wl_display* host_ = nullptr;
  wl_event_source* fd_src_ = nullptr;
  wl_registry* reg_ = nullptr;
  wl_compositor* hcomp_ = nullptr;
  xdg_wm_base* hwm_ = nullptr;
  wl_seat* hseat_ = nullptr;
  wl_pointer* hptr_ = nullptr;
  wl_keyboard* hkbd_ = nullptr;

  wl_surface* hsurf_ = nullptr;
  xdg_surface* hxsurf_ = nullptr;
  xdg_toplevel* htop_ = nullptr;
  wl_egl_window* eglwin_ = nullptr;
  wl_callback* frame_cb_ = nullptr;

  EGLDisplay edpy_ = EGL_NO_DISPLAY;
  EGLContext ctx_ = EGL_NO_CONTEXT;
  EGLConfig econf_ = nullptr;
  EGLSurface esurf_ = EGL_NO_SURFACE;

  Output out_;
  std::vector<Output*> outs_;
  int32_t pending_w_ = 1280, pending_h_ = 800;
  bool first_configure_ = false;
  bool pointer_inside_ = false;
  double ax_dx_ = 0, ax_dy_ = 0;
  xkb_context* xkb_ = nullptr;
  xkb_state* kbd_state_ = nullptr;
  xkb_keymap* kbd_keymap_ = nullptr;
};

// ------------------------------------------------------------------ listeners

static void reg_add(void* data, wl_registry* reg, uint32_t name,
                    const char* iface, uint32_t version) {
  static_cast<WaylandBackend*>(data)->registry_add(name, iface, version);
}
static void reg_remove(void*, wl_registry*, uint32_t) {}
static const wl_registry_listener reg_listener = {reg_add, reg_remove};

static void xsurf_configure(void* data, xdg_surface* s, uint32_t serial) {
  static_cast<WaylandBackend*>(data)->surface_configure(serial);
}
static const xdg_surface_listener xsurf_listener = {xsurf_configure};

static void top_configure(void* data, xdg_toplevel*, int32_t w, int32_t h,
                          wl_array* states) {
  static_cast<WaylandBackend*>(data)->toplevel_configure(w, h, states);
}
static void top_close(void* data, xdg_toplevel*) {
  // user closed the host window; ask the compositor to exit
  exit(0);
}
static const xdg_toplevel_listener top_listener = {top_configure, top_close,
                                                   nullptr, nullptr};

static void frame_done(void* data, wl_callback* cb, uint32_t t) {
  auto* b = static_cast<WaylandBackend*>(data);
  wl_callback_destroy(cb);
  b->frame_cb_ = nullptr;
  b->frame_done(t);
}
static const wl_callback_listener frame_listener = {frame_done};

static void ptr_enter(void* data, wl_pointer*, uint32_t serial,
                      wl_surface* s, wl_fixed_t x, wl_fixed_t y) {
  static_cast<WaylandBackend*>(data)->pointer_enter(serial, s, x, y);
}
static void ptr_leave(void* data, wl_pointer*, uint32_t serial,
                      wl_surface* s) {
  static_cast<WaylandBackend*>(data)->pointer_leave(serial, s);
}
static void ptr_motion(void* data, wl_pointer*, uint32_t t, wl_fixed_t x,
                       wl_fixed_t y) {
  static_cast<WaylandBackend*>(data)->pointer_motion(t, x, y);
}
static void ptr_button(void* data, wl_pointer*, uint32_t serial, uint32_t t,
                       uint32_t button, uint32_t state) {
  static_cast<WaylandBackend*>(data)->pointer_button(serial, t, button,
                                                    state);
}
static void ptr_axis(void* data, wl_pointer*, uint32_t t, uint32_t axis,
                     wl_fixed_t value) {
  static_cast<WaylandBackend*>(data)->pointer_axis(t, axis, value);
}
static void ptr_frame(void* data, wl_pointer*) {
  static_cast<WaylandBackend*>(data)->pointer_frame();
}
static void ptr_axis_source(void*, wl_pointer*, uint32_t) {}
static void ptr_axis_stop(void*, wl_pointer*, uint32_t, uint32_t) {}
static void ptr_axis_discrete(void* data, wl_pointer*, uint32_t axis,
                              int32_t discrete) {}
static const wl_pointer_listener ptr_listener = {
    ptr_enter,   ptr_leave,        ptr_motion,     ptr_button,
    ptr_axis,    ptr_frame,        ptr_axis_source, ptr_axis_stop,
    ptr_axis_discrete,
};

static void kbd_keymap(void* data, wl_keyboard*, uint32_t format, int32_t fd,
                       uint32_t size) {
  static_cast<WaylandBackend*>(data)->keyboard_keymap(format, fd, size);
}
static void kbd_enter(void* data, wl_keyboard*, uint32_t serial,
                      wl_surface* s, wl_array* keys) {
  static_cast<WaylandBackend*>(data)->keyboard_enter(serial, s, keys);
}
static void kbd_leave(void* data, wl_keyboard*, uint32_t serial,
                      wl_surface* s) {
  static_cast<WaylandBackend*>(data)->keyboard_leave(serial, s);
}
static void kbd_key(void* data, wl_keyboard*, uint32_t serial, uint32_t t,
                    uint32_t key, uint32_t state) {
  static_cast<WaylandBackend*>(data)->keyboard_key(serial, t, key, state);
}
static void kbd_mods(void* data, wl_keyboard*, uint32_t serial, uint32_t dep,
                     uint32_t lat, uint32_t lock, uint32_t grp) {
  static_cast<WaylandBackend*>(data)->keyboard_modifiers(serial, dep, lat,
                                                        lock, grp);
}
static void kbd_repeat(void*, wl_keyboard*, int32_t, int32_t) {}
static const wl_keyboard_listener kbd_listener = {
    kbd_keymap, kbd_enter, kbd_leave, kbd_key, kbd_mods, kbd_repeat,
};

static void seat_caps(void* data, wl_seat* seat, uint32_t caps) {
  auto* b = static_cast<WaylandBackend*>(data);
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !b->hptr_) {
    b->hptr_ = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(b->hptr_, &ptr_listener, b);
  }
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !b->hkbd_) {
    b->hkbd_ = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(b->hkbd_, &kbd_listener, b);
  }
}
static void seat_name(void*, wl_seat*, const char*) {}
static const wl_seat_listener seat_listener = {seat_caps, seat_name};

// ---------------------------------------------------------------------- impl

void WaylandBackend::registry_add(uint32_t name, const char* iface,
                                  uint32_t version) {
  if (strcmp(iface, wl_compositor_interface.name) == 0) {
    hcomp_ = static_cast<wl_compositor*>(
        wl_registry_bind(reg_, name, &wl_compositor_interface, 4));
  } else if (strcmp(iface, xdg_wm_base_interface.name) == 0) {
    hwm_ = static_cast<xdg_wm_base*>(
        wl_registry_bind(reg_, name, &xdg_wm_base_interface, 3));
    xdg_wm_base_add_listener(hwm_, nullptr, nullptr);
  } else if (strcmp(iface, wl_seat_interface.name) == 0) {
    hseat_ = static_cast<wl_seat*>(
        wl_registry_bind(reg_, name, &wl_seat_interface, 5));
    wl_seat_add_listener(hseat_, &seat_listener, this);
  }
}

bool WaylandBackend::init(wl_event_loop* loop, Callbacks cb) {
  loop_ = loop;
  cb_ = std::move(cb);
  const char* host_name = getenv("OASIS_HOST_DISPLAY");
  host_ = wl_display_connect(host_name && host_name[0] ? host_name
                                                     : nullptr);
  if (!host_) {
    LOGE("cannot connect to host wayland display");
    return false;
  }
  reg_ = wl_display_get_registry(host_);
  wl_registry_add_listener(reg_, &reg_listener, this);
  wl_display_roundtrip(host_);
  if (!hcomp_ || !hwm_ || !hseat_) {
    LOGE("host missing required globals (compositor/xdg_wm_base/seat)");
    return false;
  }

  out_.name = "WL-1";
  out_.make = "Oasis";
  out_.model = "Nested output";
  out_.width = pending_w_;
  out_.height = pending_h_;
  out_.phys_w = 344;
  out_.phys_h = 194;
  out_.refresh = 60000;
  outs_.push_back(&out_);

  hsurf_ = wl_compositor_create_surface(hcomp_);
  hxsurf_ = xdg_wm_base_get_xdg_surface(hwm_, hsurf_);
  xdg_surface_add_listener(hxsurf_, &xsurf_listener, this);
  htop_ = xdg_surface_get_toplevel(hxsurf_);
  xdg_toplevel_add_listener(htop_, &top_listener, this);
  xdg_toplevel_set_title(htop_, "Oasis");
  xdg_toplevel_set_app_id(htop_, "dev.oasis.compositor");
  xdg_toplevel_set_min_size(htop_, 480, 360);
  wl_surface_commit(hsurf_);
  wl_display_roundtrip(host_);

  // host fd -> our event loop
  int fd = wl_display_get_fd(host_);
  fd_src_ = wl_event_loop_add_fd(
      loop_, fd, WL_EVENT_READABLE,
      [](int, uint32_t, void* data) -> int {
        auto* b = static_cast<WaylandBackend*>(data);
        while (wl_display_prepare_read(b->host_) != 0)
          wl_display_dispatch_pending(b->host_);
        if (wl_display_read_events(b->host_) < 0) {
          LOGE("host display read failed");
          exit(1);
        }
        wl_display_dispatch_pending(b->host_);
        return 0;
      },
      this);

  return true;
}

// Lazily creates EGL state on first frame so the EGL surface matches the
// actual configured window size.
static bool egl_init(WaylandBackend* b) {
  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  if (get_platform)
    b->edpy_ = get_platform(EGL_PLATFORM_WAYLAND_EXT, b->host_, nullptr);
  if (b->edpy_ == EGL_NO_DISPLAY)
    b->edpy_ = eglGetDisplay((EGLNativeDisplayType)b->host_);
  if (!eglInitialize(b->edpy_, nullptr, nullptr)) {
    LOGE("eglInitialize failed");
    return false;
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                    EGL_ALPHA_SIZE, 8, EGL_NONE};
  EGLint n = 0;
  eglChooseConfig(b->edpy_, attrs, &b->econf_, 1, &n);
  if (n < 1) {
    LOGE("no EGL config");
    return false;
  }
  EGLint cattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  b->ctx_ = eglCreateContext(b->edpy_, b->econf_, EGL_NO_CONTEXT, cattr);
  b->eglwin_ = wl_egl_window_create(b->hsurf_, b->out_.width, b->out_.height);
  b->esurf_ = eglCreateWindowSurface(b->edpy_, b->econf_,
                                     (EGLNativeWindowType)b->eglwin_,
                                     nullptr);
  return b->ctx_ != EGL_NO_CONTEXT && b->esurf_ != EGL_NO_SURFACE;
}

EGLContext WaylandBackend::new_context() {
  EGLint cattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  return eglCreateContext(edpy_, econf_, ctx_, cattr);
}

void WaylandBackend::surface_configure(uint32_t serial) {
  xdg_surface_ack_configure(hxsurf_, serial);
  if (pending_w_ != out_.width || pending_h_ != out_.height) {
    out_.width = pending_w_;
    out_.height = pending_h_;
    if (eglwin_)
      wl_egl_window_resize(eglwin_, out_.width, out_.height, 0, 0);
    cb_.output_resized(&out_, out_.width, out_.height);
  }
  if (!first_configure_) {
    first_configure_ = true;
    if (!egl_init(this)) {
      LOGE("egl init failed");
      exit(1);
    }
  }
  // force a redraw on every host configure
  cb_.frame(&out_);
}

void WaylandBackend::toplevel_configure(int32_t w, int32_t h, wl_array*) {
  if (w > 0 && h > 0) {
    pending_w_ = w;
    pending_h_ = h;
  }
}

void WaylandBackend::begin_frame(Output* o) {
  if (eglGetCurrentContext() != ctx_)
    eglMakeCurrent(edpy_, esurf_, esurf_, ctx_);
}

void WaylandBackend::end_frame(Output* o) {
  eglSwapBuffers(edpy_, esurf_);
  if (!frame_cb_) {
    frame_cb_ = wl_surface_frame(hsurf_);
    wl_callback_add_listener(frame_cb_, &frame_listener, this);
  }
  wl_surface_commit(hsurf_);
}

void WaylandBackend::schedule_frame(Output* o) {
  if (!frame_cb_ && hsurf_) {
    frame_cb_ = wl_surface_frame(hsurf_);
    wl_callback_add_listener(frame_cb_, &frame_listener, this);
    wl_surface_commit(hsurf_);
  }
}

void WaylandBackend::frame_done(uint32_t t) {
  cb_.frame(&out_);
  // when nothing was dirty the render was skipped; keep the pump alive
  if (!frame_cb_) schedule_frame(&out_);
}

void WaylandBackend::pointer_enter(uint32_t serial, wl_surface* s,
                                   wl_fixed_t x, wl_fixed_t y) {
  pointer_inside_ = true;
  // hide the host cursor; we draw our own
  wl_pointer_set_cursor(hptr_, serial, nullptr, 0, 0);
  cb_.pointer_motion(wl_fixed_to_double(x), wl_fixed_to_double(y), now_msec());
}

void WaylandBackend::pointer_leave(uint32_t serial, wl_surface* s) {
  pointer_inside_ = false;
}

void WaylandBackend::pointer_motion(uint32_t t, wl_fixed_t x, wl_fixed_t y) {
  cb_.pointer_motion(wl_fixed_to_double(x), wl_fixed_to_double(y), t);
}

void WaylandBackend::pointer_button(uint32_t serial, uint32_t t,
                                    uint32_t button, uint32_t state) {
  cb_.pointer_button(button, state == WL_POINTER_BUTTON_STATE_PRESSED, t);
}

void WaylandBackend::pointer_axis(uint32_t t, uint32_t axis,
                                  wl_fixed_t value) {
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    ax_dy_ += wl_fixed_to_double(value);
  else
    ax_dx_ += wl_fixed_to_double(value);
}

void WaylandBackend::pointer_frame() {
  if (ax_dx_ != 0 || ax_dy_ != 0) {
    cb_.pointer_axis(ax_dx_, ax_dy_, now_msec());
    ax_dx_ = ax_dy_ = 0;
  }
}

void WaylandBackend::keyboard_keymap(uint32_t format, int32_t fd,
                                     uint32_t size) {
  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    close(fd);
    return;
  }
  char* data = static_cast<char*>(
      mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (data != MAP_FAILED) {
    std::string str(data, strnlen(data, size));
    if (!xkb_) xkb_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap* km = xkb_keymap_new_from_string(
        xkb_, str.c_str(), XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (km) {
      if (kbd_keymap_) xkb_keymap_unref(kbd_keymap_);
      kbd_keymap_ = km;
      if (kbd_state_) xkb_state_unref(kbd_state_);
      kbd_state_ = xkb_state_new(km);
      cb_.keymap(str);
    }
    munmap(data, size);
  }
  close(fd);
}

void WaylandBackend::keyboard_enter(uint32_t serial, wl_surface* s,
                                    wl_array* keys) {}

void WaylandBackend::keyboard_leave(uint32_t serial, wl_surface* s) {}

void WaylandBackend::keyboard_key(uint32_t serial, uint32_t t, uint32_t key,
                                  uint32_t state) {
  bool press = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  if (kbd_state_)
    xkb_state_update_key(kbd_state_, key + 8,
                         press ? XKB_KEY_DOWN : XKB_KEY_UP);
  cb_.key(key, press, t);
}

void WaylandBackend::keyboard_modifiers(uint32_t serial, uint32_t dep,
                                        uint32_t lat, uint32_t lock,
                                        uint32_t grp) {
  if (kbd_state_) {
    xkb_state_update_mask(kbd_state_, dep, lat, lock, 0, 0, grp);
  }
  if (cb_.modifiers) cb_.modifiers(dep, lat, lock, grp);
}

Backend* create_wayland_backend() { return new WaylandBackend(); }

}  // namespace oasis
