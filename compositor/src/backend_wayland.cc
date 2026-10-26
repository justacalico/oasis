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
#include "wl.h"
#include "xdg-shell-client.h"

namespace oasis {

class WaylandBackend : public Backend {
 public:
  ~WaylandBackend() override;
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
  void registry_add(wl_registry*, uint32_t name, const char* iface,
                    uint32_t version);
  void registry_remove(wl_registry*, uint32_t) {}
  void toplevel_configure(xdg_toplevel*, int32_t w, int32_t h,
                          wl_array* states);
  void toplevel_close(xdg_toplevel*) { cb_.quit(); }
  void surface_configure(xdg_surface*, uint32_t serial);
  void frame_done(wl_callback*, uint32_t t);
  void pointer_enter(wl_pointer*, uint32_t serial, wl_surface* s,
                     wl_fixed_t x, wl_fixed_t y);
  void pointer_leave(wl_pointer*, uint32_t serial, wl_surface* s);
  void pointer_motion(wl_pointer*, uint32_t t, wl_fixed_t x, wl_fixed_t y);
  void pointer_button(wl_pointer*, uint32_t serial, uint32_t t,
                      uint32_t button, uint32_t state);
  void pointer_axis(wl_pointer*, uint32_t t, uint32_t axis, wl_fixed_t value);
  void pointer_frame(wl_pointer*);
  void pointer_axis_source(wl_pointer*, uint32_t) {}
  void pointer_axis_stop(wl_pointer*, uint32_t, uint32_t) {}
  void pointer_axis_discrete(wl_pointer*, uint32_t, int32_t) {}
  void keyboard_keymap(wl_keyboard*, uint32_t format, int32_t fd,
                       uint32_t size);
  void keyboard_enter(wl_keyboard*, uint32_t serial, wl_surface* s,
                      wl_array* keys) {}
  void keyboard_leave(wl_keyboard*, uint32_t serial, wl_surface* s) {}
  void keyboard_key(wl_keyboard*, uint32_t serial, uint32_t t, uint32_t key,
                    uint32_t state);
  void keyboard_modifiers(wl_keyboard*, uint32_t serial, uint32_t dep,
                          uint32_t lat, uint32_t lock, uint32_t grp);
  void keyboard_repeat(wl_keyboard*, int32_t, int32_t) {}
  void seat_caps(wl_seat* seat, uint32_t caps);
  void seat_name(wl_seat*, const char*) {}

 private:
  bool init_egl();
  int dispatch_host();

  Callbacks cb_;
  wl_event_loop* loop_ = nullptr;
  wl_display* host_ = nullptr;
  std::unique_ptr<EventSource> fd_src_;
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

static const wl_registry_listener reg_listener = {
    LREQ(WaylandBackend, registry_add),
    LREQ(WaylandBackend, registry_remove),
};

static const xdg_surface_listener xsurf_listener = {
    LREQ(WaylandBackend, surface_configure),
};

static const xdg_toplevel_listener top_listener = {
    LREQ(WaylandBackend, toplevel_configure),
    LREQ(WaylandBackend, toplevel_close),
    nullptr,
    nullptr,
};

static const wl_callback_listener frame_listener = {
    LREQ(WaylandBackend, frame_done),
};

static const wl_pointer_listener ptr_listener = {
    LREQ(WaylandBackend, pointer_enter),
    LREQ(WaylandBackend, pointer_leave),
    LREQ(WaylandBackend, pointer_motion),
    LREQ(WaylandBackend, pointer_button),
    LREQ(WaylandBackend, pointer_axis),
    LREQ(WaylandBackend, pointer_frame),
    LREQ(WaylandBackend, pointer_axis_source),
    LREQ(WaylandBackend, pointer_axis_stop),
    LREQ(WaylandBackend, pointer_axis_discrete),
};

static const wl_keyboard_listener kbd_listener = {
    LREQ(WaylandBackend, keyboard_keymap),
    LREQ(WaylandBackend, keyboard_enter),
    LREQ(WaylandBackend, keyboard_leave),
    LREQ(WaylandBackend, keyboard_key),
    LREQ(WaylandBackend, keyboard_modifiers),
    LREQ(WaylandBackend, keyboard_repeat),
};

static const wl_seat_listener seat_listener = {
    LREQ(WaylandBackend, seat_caps), LREQ(WaylandBackend, seat_name),
};

// ---------------------------------------------------------------------- impl

void WaylandBackend::registry_add(wl_registry*, uint32_t name,
                                  const char* iface, uint32_t version) {
  if (strcmp(iface, wl_compositor_interface.name) == 0) {
    hcomp_ = static_cast<wl_compositor*>(
        wl_registry_bind(reg_, name, &wl_compositor_interface, 4));
  } else if (strcmp(iface, xdg_wm_base_interface.name) == 0) {
    hwm_ = static_cast<xdg_wm_base*>(
        wl_registry_bind(reg_, name, &xdg_wm_base_interface, 3));
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
  host_ = wl_display_connect(host_name && host_name[0] ? host_name : nullptr);
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
  fd_src_ = EventSource::watch(
      loop_, fd, WL_EVENT_READABLE,
      [this](int, uint32_t) { return dispatch_host(); });

  return true;
}

int WaylandBackend::dispatch_host() {
  while (wl_display_prepare_read(host_) != 0)
    wl_display_dispatch_pending(host_);
  if (wl_display_read_events(host_) < 0) {
    LOGE("host display read failed");
    cb_.quit();
    return -1;
  }
  wl_display_dispatch_pending(host_);
  return 0;
}

// Lazily creates EGL state on first frame so the EGL surface matches the
// actual configured window size.
bool WaylandBackend::init_egl() {
  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  if (get_platform)
    edpy_ = get_platform(EGL_PLATFORM_WAYLAND_EXT, host_, nullptr);
  if (edpy_ == EGL_NO_DISPLAY)
    edpy_ = eglGetDisplay((EGLNativeDisplayType)host_);
  if (!eglInitialize(edpy_, nullptr, nullptr)) {
    LOGE("eglInitialize failed");
    return false;
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                    EGL_ALPHA_SIZE, 8, EGL_NONE};
  EGLint n = 0;
  eglChooseConfig(edpy_, attrs, &econf_, 1, &n);
  if (n < 1) {
    LOGE("no EGL config");
    return false;
  }
  EGLint cattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  ctx_ = eglCreateContext(edpy_, econf_, EGL_NO_CONTEXT, cattr);
  eglwin_ = wl_egl_window_create(hsurf_, out_.width, out_.height);
  esurf_ = eglCreateWindowSurface(edpy_, econf_, (EGLNativeWindowType)eglwin_,
                                  nullptr);
  return ctx_ != EGL_NO_CONTEXT && esurf_ != EGL_NO_SURFACE;
}

EGLContext WaylandBackend::new_context() {
  EGLint cattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  return eglCreateContext(edpy_, econf_, ctx_, cattr);
}

void WaylandBackend::surface_configure(xdg_surface*, uint32_t serial) {
  xdg_surface_ack_configure(hxsurf_, serial);
  if (pending_w_ != out_.width || pending_h_ != out_.height) {
    out_.width = pending_w_;
    out_.height = pending_h_;
    if (eglwin_) wl_egl_window_resize(eglwin_, out_.width, out_.height, 0, 0);
    cb_.output_resized(&out_, out_.width, out_.height);
  }
  if (!first_configure_) {
    first_configure_ = true;
    if (!init_egl()) {
      LOGE("egl init failed");
      cb_.quit();
      return;
    }
  }
  // force a redraw on every host configure
  cb_.frame(&out_);
}

void WaylandBackend::toplevel_configure(xdg_toplevel*, int32_t w, int32_t h,
                                        wl_array*) {
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

void WaylandBackend::frame_done(wl_callback* cb, uint32_t t) {
  wl_callback_destroy(cb);
  frame_cb_ = nullptr;
  cb_.frame(&out_);
  // when nothing was dirty the render was skipped; keep the pump alive
  if (!frame_cb_) schedule_frame(&out_);
}

void WaylandBackend::pointer_enter(wl_pointer*, uint32_t serial, wl_surface*,
                                   wl_fixed_t x, wl_fixed_t y) {
  pointer_inside_ = true;
  // hide the host cursor; we draw our own
  wl_pointer_set_cursor(hptr_, serial, nullptr, 0, 0);
  cb_.pointer_motion(wl_fixed_to_double(x), wl_fixed_to_double(y), now_msec());
}

void WaylandBackend::pointer_leave(wl_pointer*, uint32_t serial, wl_surface*) {
  pointer_inside_ = false;
}

void WaylandBackend::pointer_motion(wl_pointer*, uint32_t t, wl_fixed_t x,
                                    wl_fixed_t y) {
  cb_.pointer_motion(wl_fixed_to_double(x), wl_fixed_to_double(y), t);
}

void WaylandBackend::pointer_button(wl_pointer*, uint32_t serial, uint32_t t,
                                    uint32_t button, uint32_t state) {
  cb_.pointer_button(button, state == WL_POINTER_BUTTON_STATE_PRESSED, t);
}

void WaylandBackend::pointer_axis(wl_pointer*, uint32_t t, uint32_t axis,
                                  wl_fixed_t value) {
  if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
    ax_dy_ += wl_fixed_to_double(value);
  else
    ax_dx_ += wl_fixed_to_double(value);
}

void WaylandBackend::pointer_frame(wl_pointer*) {
  if (ax_dx_ != 0 || ax_dy_ != 0) {
    cb_.pointer_axis(ax_dx_, ax_dy_, now_msec());
    ax_dx_ = ax_dy_ = 0;
  }
}

void WaylandBackend::keyboard_keymap(wl_keyboard*, uint32_t format, int32_t fd,
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

void WaylandBackend::keyboard_key(wl_keyboard*, uint32_t serial, uint32_t t,
                                  uint32_t key, uint32_t state) {
  bool press = state == WL_KEYBOARD_KEY_STATE_PRESSED;
  if (kbd_state_)
    xkb_state_update_key(kbd_state_, key + 8,
                         press ? XKB_KEY_DOWN : XKB_KEY_UP);
  cb_.key(key, press, t);
}

void WaylandBackend::keyboard_modifiers(wl_keyboard*, uint32_t serial,
                                        uint32_t dep, uint32_t lat,
                                        uint32_t lock, uint32_t grp) {
  if (kbd_state_)
    xkb_state_update_mask(kbd_state_, dep, lat, lock, 0, 0, grp);
  if (cb_.modifiers) cb_.modifiers(dep, lat, lock, grp);
}

void WaylandBackend::seat_caps(wl_seat* seat, uint32_t caps) {
  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !hptr_) {
    hptr_ = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(hptr_, &ptr_listener, this);
  }
  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !hkbd_) {
    hkbd_ = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(hkbd_, &kbd_listener, this);
  }
}

WaylandBackend::~WaylandBackend() {
  fd_src_.reset();
  if (htop_) xdg_toplevel_destroy(htop_);
  if (hxsurf_) xdg_surface_destroy(hxsurf_);
  if (eglwin_) wl_egl_window_destroy(eglwin_);
  if (hsurf_) wl_surface_destroy(hsurf_);
  if (hptr_) wl_pointer_destroy(hptr_);
  if (hkbd_) wl_keyboard_destroy(hkbd_);
  if (hseat_) wl_seat_destroy(hseat_);
  if (hwm_) xdg_wm_base_destroy(hwm_);
  if (hcomp_) wl_compositor_destroy(hcomp_);
  if (reg_) wl_registry_destroy(reg_);
  if (host_) wl_display_disconnect(host_);
  if (kbd_state_) xkb_state_unref(kbd_state_);
  if (kbd_keymap_) xkb_keymap_unref(kbd_keymap_);
  if (xkb_) xkb_context_unref(xkb_);
}

std::unique_ptr<Backend> create_wayland_backend() {
  return std::make_unique<WaylandBackend>();
}

}  // namespace oasis
