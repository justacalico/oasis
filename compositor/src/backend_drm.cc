// KMS/DRM backend: runs the compositor on bare hardware using libseat for
// session/device access, GBM+EGL for buffers, and libinput for devices.
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <gbm.h>
#include <libinput.h>
extern "C" {
#include <libseat.h>
}
#include <libudev.h>
#include <xkbcommon/xkbcommon.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include "backend.h"
#include "log.h"
#include "util.h"

namespace oasis {

class DrmBackend;

struct DrmOutput : Backend::Output {
  gbm_surface* gbm_surf = nullptr;
  EGLSurface esurf = EGL_NO_SURFACE;
  gbm_bo* bo = nullptr;
  gbm_bo* next_bo = nullptr;
  uint32_t fb_id = 0;
  uint32_t conn_id = 0, crtc_id = 0;
  drmModeCrtc* saved_crtc = nullptr;
  bool flip_pending = false;
  DrmBackend* backend = nullptr;
};

class DrmBackend : public Backend {
 public:
  ~DrmBackend() override;
  bool init(wl_event_loop* loop, Callbacks cb) override;
  EGLDisplay egl_display() const override { return edpy_; }
  EGLContext egl_context() const override { return ctx_; }
  EGLConfig egl_config() const override { return econf_; }
  EGLContext new_context() override;
  const std::vector<Output*>& outputs() const override { return outs_; }
  void begin_frame(Output* o) override;
  void end_frame(Output* o) override;
  void schedule_frame(Output* o) override;

  void seat_enabled();
  void seat_disabled();
  void dispatch_seat();
  void dispatch_drm();
  void dispatch_input();
  void add_input_device(libinput_device* dev);
  void page_flip_done(DrmOutput* out);

  Callbacks cb_;
  wl_event_loop* loop_ = nullptr;
  libseat* seat_ = nullptr;
  libinput* li_ = nullptr;
  int drm_fd_ = -1;
  gbm_device* gbm_ = nullptr;
  EGLDisplay edpy_ = EGL_NO_DISPLAY;
  EGLConfig econf_ = nullptr;
  EGLContext ctx_ = EGL_NO_CONTEXT;
  wl_event_source* seat_src_ = nullptr;
  wl_event_source* drm_src_ = nullptr;
  wl_event_source* input_src_ = nullptr;
  bool active_ = false;
  bool initial_modeset_done_ = false;
  double px_ = 0, py_ = 0;
  std::vector<Output*> outs_;
  std::vector<DrmOutput*> drouts_;
  xkb_context* xkb_ = nullptr;
  xkb_state* kbd_state_ = nullptr;
  xkb_keymap* kbd_keymap_ = nullptr;
};

// ------------------------------------------------------------------ libseat

static void seat_enable(libseat* seat, void* data) {
  static_cast<DrmBackend*>(data)->seat_enabled();
}
static void seat_disable(libseat* seat, void* data) {
  auto* b = static_cast<DrmBackend*>(data);
  b->seat_disabled();
  libseat_disable_seat(seat);
}
static const libseat_seat_listener seat_listener = {seat_enable,
                                                    seat_disable};

// ---------------------------------------------------------------- libinput

static int li_open_restricted(const char* path, int flags, void* ud) {
  auto* b = static_cast<DrmBackend*>(ud);
  int fd = -1;
  if (libseat_open_device(b->seat_, path, &fd) < 0) return -1;
  return fd;
}
static void li_close_restricted(int fd, void* ud) {
  auto* b = static_cast<DrmBackend*>(ud);
  libseat_close_device(b->seat_, fd);
}
static const libinput_interface li_interface = {li_open_restricted,
                                                li_close_restricted};

// ------------------------------------------------------------- EGL on GBM

static bool egl_init_gbm(DrmBackend* b) {
  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress(
          "eglGetPlatformDisplayEXT");
  if (get_platform)
    b->edpy_ = get_platform(EGL_PLATFORM_GBM_KHR, b->gbm_, nullptr);
  if (b->edpy_ == EGL_NO_DISPLAY)
    b->edpy_ = eglGetDisplay((EGLNativeDisplayType)b->gbm_);
  if (!eglInitialize(b->edpy_, nullptr, nullptr)) {
    LOGE("eglInitialize (gbm) failed");
    return false;
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLint attrs[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                    EGL_ALPHA_SIZE, 0, EGL_NONE};
  EGLint n = 0;
  eglChooseConfig(b->edpy_, attrs, &b->econf_, 1, &n);
  if (n < 1) {
    LOGE("no EGL config for gbm");
    return false;
  }
  EGLint cattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  b->ctx_ = eglCreateContext(b->edpy_, b->econf_, EGL_NO_CONTEXT, cattr);
  return b->ctx_ != EGL_NO_CONTEXT;
}

EGLContext DrmBackend::new_context() {
  EGLint cattr[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  return eglCreateContext(edpy_, econf_, ctx_, cattr);
}

// ------------------------------------------------------------ connectors

static void init_outputs(DrmBackend* b) {
  drmModeRes* res = drmModeGetResources(b->drm_fd_);
  if (!res) {
    LOGE("drmModeGetResources failed");
    return;
  }
  std::vector<bool> crtc_used(res->count_crtcs, false);
  int out_idx = 0;
  int32_t lay_x = 0;
  for (int i = 0; i < res->count_connectors; i++) {
    drmModeConnector* conn =
        drmModeGetConnector(b->drm_fd_, res->connectors[i]);
    if (!conn || conn->connection != DRM_MODE_CONNECTED ||
        conn->count_modes == 0) {
      if (conn) drmModeFreeConnector(conn);
      continue;
    }
    drmModeModeInfo* mode = &conn->modes[0];
    for (int m = 0; m < conn->count_modes; m++)
      if (conn->modes[m].type & DRM_MODE_TYPE_PREFERRED)
        mode = &conn->modes[m];

    // find a crtc: prefer the connector's encoder, else first free
    uint32_t crtc_id = 0;
    if (conn->encoder_id) {
      drmModeEncoder* enc =
          drmModeGetEncoder(b->drm_fd_, conn->encoder_id);
      if (enc) {
        if (enc->crtc_id) crtc_id = enc->crtc_id;
        drmModeFreeEncoder(enc);
      }
    }
    if (!crtc_id) {
      for (int c = 0; c < res->count_crtcs && !crtc_id; c++)
        if (!crtc_used[c]) crtc_id = res->crtcs[c];
    }
    for (int c = 0; c < res->count_crtcs; c++)
      if (res->crtcs[c] == crtc_id) crtc_used[c] = true;

    auto* out = new DrmOutput();
    out->backend = b;
    out->conn_id = conn->connector_id;
    out->crtc_id = crtc_id;
    out->saved_crtc = drmModeGetCrtc(b->drm_fd_, crtc_id);
    out->width = mode->hdisplay;
    out->height = mode->vdisplay;
    out->phys_w = conn->mmWidth;
    out->phys_h = conn->mmHeight;
    out->refresh = mode->vrefresh * 1000;
    out->x = lay_x;
    lay_x += out->width;
    char nm[32];
    snprintf(nm, sizeof(nm), "%s-%u",
             drmModeGetConnectorTypeName(conn->connector_type),
             conn->connector_type_id);
    out->name = nm;
    out->make = "DRM";
    out->model = nm;

    out->gbm_surf = gbm_surface_create(b->gbm_, mode->hdisplay,
                                     mode->vdisplay, GBM_FORMAT_XRGB8888,
                                     GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    out->esurf = eglCreateWindowSurface(b->edpy_, b->econf_,
                                        (EGLNativeWindowType)out->gbm_surf,
                                        nullptr);
    if (out->esurf == EGL_NO_SURFACE) {
      LOGE("eglCreateWindowSurface failed for %s", nm);
      drmModeFreeConnector(conn);
      delete out;
      continue;
    }
    b->outs_.push_back(out);
    b->drouts_.push_back(out);
    out_idx++;
    drmModeFreeConnector(conn);
  }
  drmModeFreeResources(res);
  LOGI("drm: %d output(s)", (int)b->drouts_.size());
}

// ------------------------------------------------------------ flip events

static void drm_fb_event(int fd, unsigned int frame, unsigned int sec,
                         unsigned int usec, unsigned int crtc_id,
                         void* data) {
  auto* out = static_cast<DrmOutput*>(data);
  out->flip_pending = false;
  if (out->bo) {
    gbm_surface_release_buffer(out->gbm_surf, out->bo);
  }
  out->bo = out->next_bo;
  out->next_bo = nullptr;
  out->backend->cb_.frame(out);
}

void DrmBackend::dispatch_drm() {
  drmEventContext ctx = {};
  ctx.version = 4;
  ctx.page_flip_handler2 = drm_fb_event;
  drmHandleEvent(drm_fd_, &ctx);
}

void DrmBackend::begin_frame(Output* o) {
  auto* out = static_cast<DrmOutput*>(o);
  eglMakeCurrent(edpy_, out->esurf, out->esurf, ctx_);
}

void DrmBackend::end_frame(Output* o) {
  auto* out = static_cast<DrmOutput*>(o);
  eglSwapBuffers(edpy_, out->esurf);
  gbm_bo* bo = gbm_surface_lock_front_buffer(out->gbm_surf);
  if (!bo) {
    LOGE("lock_front_buffer failed");
    return;
  }
  uint32_t handle = gbm_bo_get_handle(bo).u32;
  uint32_t stride = gbm_bo_get_stride(bo);
  uint32_t fb = 0;
  if (drmModeAddFB(drm_fd_, out->width, out->height, 24, 32, stride,
                   handle, &fb) < 0) {
    LOGE("drmModeAddFB failed");
    gbm_surface_release_buffer(out->gbm_surf, bo);
    return;
  }
  if (!initial_modeset_done_ || out->saved_crtc == nullptr ||
      out->saved_crtc->buffer_id == 0) {
    if (drmModeSetCrtc(drm_fd_, out->crtc_id, fb, out->x, out->y,
                       &out->conn_id, 1, nullptr) < 0) {
      // fall through to page flip path anyway
    }
    initial_modeset_done_ = true;
  }
  out->next_bo = bo;
  if (drmModePageFlip(drm_fd_, out->crtc_id, fb,
                      DRM_MODE_PAGE_FLIP_EVENT, out) < 0) {
    LOGE("page flip failed");
    gbm_surface_release_buffer(out->gbm_surf, bo);
    out->next_bo = nullptr;
    return;
  }
  out->fb_id = fb;
  out->flip_pending = true;
}

void DrmBackend::schedule_frame(Output* o) {
  auto* out = static_cast<DrmOutput*>(o);
  if (!out->flip_pending && active_) cb_.frame(o);
}

// ----------------------------------------------------------------- input

void DrmBackend::dispatch_input() {
  libinput_dispatch(li_);
  libinput_event* ev;
  while ((ev = libinput_get_event(li_)) != nullptr) {
    uint32_t t = 0;
    switch (libinput_event_get_type(ev)) {
      case LIBINPUT_EVENT_POINTER_MOTION:
      case LIBINPUT_EVENT_POINTER_BUTTON:
      case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
        t = libinput_event_pointer_get_time_usec(
            libinput_event_get_pointer_event(ev)) / 1000;
        break;
      case LIBINPUT_EVENT_KEYBOARD_KEY:
        t = libinput_event_keyboard_get_time_usec(
            libinput_event_get_keyboard_event(ev)) / 1000;
        break;
      default:
        break;
    }
    switch (libinput_event_get_type(ev)) {
      case LIBINPUT_EVENT_POINTER_MOTION: {
        auto* e = libinput_event_get_pointer_event(ev);
        px_ += libinput_event_pointer_get_dx(e);
        py_ += libinput_event_pointer_get_dy(e);
        double maxx = 0, maxy = 0;
        for (auto* o : outs_) {
          maxx = std::max(maxx, (double)(o->x + o->width));
          maxy = std::max(maxy, (double)(o->y + o->height));
        }
        px_ = std::clamp(px_, 0.0, std::max(0.0, maxx - 1));
        py_ = std::clamp(py_, 0.0, std::max(0.0, maxy - 1));
        cb_.pointer_motion(px_, py_, t / 1000);
        break;
      }
      case LIBINPUT_EVENT_POINTER_BUTTON: {
        auto* e = libinput_event_get_pointer_event(ev);
        cb_.pointer_button(
            libinput_event_pointer_get_button(e),
            libinput_event_pointer_get_button_state(e) ==
                LIBINPUT_BUTTON_STATE_PRESSED,
            t / 1000);
        break;
      }
      case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL: {
        auto* e = libinput_event_get_pointer_event(ev);
        double dx = libinput_event_pointer_get_scroll_value(
            e, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
        double dy = libinput_event_pointer_get_scroll_value(
            e, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
        if (dx || dy) cb_.pointer_axis(dx * 15.0, dy * 15.0, t / 1000);
        break;
      }
      case LIBINPUT_EVENT_KEYBOARD_KEY: {
        auto* e = libinput_event_get_keyboard_event(ev);
        uint32_t key = libinput_event_keyboard_get_key(e);
        bool press = libinput_event_keyboard_get_key_state(e) ==
                     LIBINPUT_KEY_STATE_PRESSED;
        if (kbd_state_) {
          xkb_state_update_key(kbd_state_, key + 8,
                               press ? XKB_KEY_DOWN : XKB_KEY_UP);
          if (cb_.modifiers)
            cb_.modifiers(xkb_state_serialize_mods(
                              kbd_state_, XKB_STATE_MODS_DEPRESSED),
                          xkb_state_serialize_mods(kbd_state_,
                                                   XKB_STATE_MODS_LATCHED),
                          xkb_state_serialize_mods(kbd_state_,
                                                   XKB_STATE_MODS_LOCKED),
                          xkb_state_serialize_layout(
                              kbd_state_, XKB_STATE_LAYOUT_EFFECTIVE));
        }
        cb_.key(key, press, t / 1000);
        break;
      }
      default:
        break;
    }
    libinput_event_destroy(ev);
  }
}

// ---------------------------------------------------------------- session

void DrmBackend::seat_enabled() {
  LOGI("seat enabled");
  active_ = true;
  if (drm_fd_ < 0) {
    // open the first usable GPU
    for (int i = 0; i < 16 && drm_fd_ < 0; i++) {
      char path[32];
      snprintf(path, sizeof(path), "/dev/dri/card%d", i);
      int fd = -1;
      if (libseat_open_device(seat_, path, &fd) >= 0 && fd >= 0) {
        drmModeRes* res = drmModeGetResources(fd);
        if (res) {
          drm_fd_ = fd;
          drmModeFreeResources(res);
        } else {
          libseat_close_device(seat_, fd);
        }
      }
    }
    if (drm_fd_ < 0) {
      LOGE("no DRM device");
      exit(1);
    }
    gbm_ = gbm_create_device(drm_fd_);
    if (!egl_init_gbm(this)) exit(1);
    init_outputs(this);
  } else {
    drmSetMaster(drm_fd_);
  }
  if (!li_) {
    li_ = libinput_udev_create_context(&li_interface, this, udev_new());
    const char* seat_name = libseat_seat_name(seat_);
    if (li_ && libinput_udev_assign_seat(li_, seat_name ? seat_name
                                                      : "seat0") == 0) {
      int li_fd = libinput_get_fd(li_);
      input_src_ = wl_event_loop_add_fd(
          loop_, li_fd, WL_EVENT_READABLE,
          [](int, uint32_t, void* data) -> int {
            static_cast<DrmBackend*>(data)->dispatch_input();
            return 0;
          },
          this);
    }
    if (!xkb_) xkb_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    kbd_keymap_ =
        xkb_keymap_new_from_names(xkb_, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
    kbd_state_ = xkb_state_new(kbd_keymap_);
  }
}

void DrmBackend::seat_disabled() {
  LOGI("seat disabled");
  active_ = false;
}

void DrmBackend::dispatch_seat() {
  if (libseat_dispatch(seat_, 0) < 0) {
    LOGE("libseat dispatch failed");
  }
}

bool DrmBackend::init(wl_event_loop* loop, Callbacks cb) {
  loop_ = loop;
  cb_ = std::move(cb);
  seat_ = libseat_open_seat(&seat_listener, this);
  if (!seat_) {
    LOGE("cannot open seat (need logind or seatd)");
    return false;
  }
  int sfd = libseat_get_fd(seat_);
  seat_src_ = wl_event_loop_add_fd(
      loop_, sfd, WL_EVENT_READABLE,
      [](int, uint32_t, void* data) -> int {
        static_cast<DrmBackend*>(data)->dispatch_seat();
        return 0;
      },
      this);
  while (!active_) {
    if (libseat_dispatch(seat_, -1) < 0) {
      LOGE("seat dispatch failed during init");
      return false;
    }
  }
  // drm fd events
  drm_src_ = wl_event_loop_add_fd(
      loop_, drm_fd_, WL_EVENT_READABLE,
      [](int, uint32_t, void* data) -> int {
        static_cast<DrmBackend*>(data)->dispatch_drm();
        return 0;
      },
      this);
  if (outs_.empty()) {
    LOGE("no connected outputs");
    return false;
  }
  px_ = outs_[0]->width / 2.0;
  py_ = outs_[0]->height / 2.0;
  return true;
}

DrmBackend::~DrmBackend() {
  for (DrmOutput* o : drouts_) {
    if (o->saved_crtc && drm_fd_ >= 0)
      drmModeSetCrtc(drm_fd_, o->saved_crtc->crtc_id,
                     o->saved_crtc->buffer_id, o->saved_crtc->x,
                     o->saved_crtc->y, &o->conn_id, 1,
                     &o->saved_crtc->mode);
    if (o->saved_crtc) drmModeFreeCrtc(o->saved_crtc);
    delete o;
  }
  if (seat_) libseat_close_seat(seat_);
}

Backend* create_drm_backend() { return new DrmBackend(); }

}  // namespace oasis
