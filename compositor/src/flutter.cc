#include "flutter.h"

#include <dlfcn.h>
#include <EGL/eglext.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <cstring>
#include <set>
#include <string>
#include <vector>
#include "backend.h"
#include "json.h"
#include "keymap.h"
#include "log.h"
#include "seat.h"
#include "server.h"
#include "surface.h"
#include "util.h"
#include "xdg.h"

namespace oasis {

static bool file_exists(const std::string& p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0;
}

static const uint8_t* map_file(const std::string& p, size_t* size) {
  int fd = open(p.c_str(), O_RDONLY);
  if (fd < 0) return nullptr;
  struct stat st;
  fstat(fd, &st);
  void* m = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if (m == MAP_FAILED) return nullptr;
  *size = st.st_size;
  return static_cast<const uint8_t*>(m);
}

struct TaskCtx {
  FlutterHost* host;
  FlutterTask task;
};

// ------------------------------------------------------- GL bridge methods

bool FlutterHost::make_current() {
  return eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, rctx_);
}

bool FlutterHost::clear_current() {
  return eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

uint32_t FlutterHost::fbo() {
  // ping-pong between two FBOs: the one being presented must stay untouched
  // while the render thread starts the next frame
  int32_t w = server_->primary_output() ? server_->primary_output()->width
                                        : 1280;
  int32_t h = server_->primary_output() ? server_->primary_output()->height
                                        : 800;
  if (fbo_w_ != w || fbo_h_ != h || fbos_[0] == 0) {
    if (fbos_[0]) {
      glDeleteFramebuffers(2, fbos_);
      glDeleteTextures(2, fbo_texs_);
      fbos_[0] = fbos_[1] = 0;
    }
    fbo_w_ = w;
    fbo_h_ = h;
    glGenTextures(2, fbo_texs_);
    glGenFramebuffers(2, fbos_);
    for (int i = 0; i < 2; i++) {
      glBindTexture(GL_TEXTURE_2D, fbo_texs_[i]);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                   GL_UNSIGNED_BYTE, nullptr);
      glBindFramebuffer(GL_FRAMEBUFFER, fbos_[i]);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, fbo_texs_[i], 0);
    }
    fbo_index_ = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    fbo_ = fbos_[0];
  }
  fbo_index_ ^= 1;
  fbo_ = fbos_[fbo_index_];
  return fbo_;
}

bool FlutterHost::present() {
  front_tex_ = fbo_texs_[fbo_index_];
  front_w_ = fbo_w_;
  front_h_ = fbo_h_;
  if (fence_ != EGL_NO_SYNC_KHR) egl_destroy_sync_(dpy_, fence_);
  if (egl_create_sync_)
    fence_ = egl_create_sync_(dpy_, EGL_SYNC_FENCE_KHR, nullptr);
  if (fence_ == EGL_NO_SYNC_KHR) glFinish();
  have_frame_ = true;
  server_->mark_dirty();
  return true;
}

bool FlutterHost::make_resource_current() {
  return eglMakeCurrent(dpy_, EGL_NO_SURFACE, EGL_NO_SURFACE, resctx_);
}

void* FlutterHost::resolve(const char* name) {
  void* p = (void*)eglGetProcAddress(name);
  if (!p) p = dlsym(lib_, name);
  return p;
}

GLuint FlutterHost::shell_texture() {
  if (!have_frame_) return 0;
  if (fence_ != EGL_NO_SYNC_KHR && egl_wait_sync_) {
    egl_wait_sync_(dpy_, fence_, 0);
  }
  return front_tex_;
}

// --------------------------------------------------------- platform channel

void FlutterHost::notify_windows() {
  std::string msg = "{\"type\":\"windows\",\"windows\":[";
  bool first = true;
  for (XdgToplevel* tl : server_->toplevels) {
    if (!first) msg += ',';
    first = false;
    msg += "{\"id\":" + std::to_string(tl->id);
    msg += ",\"title\":\"" + json_escape(tl->title) + "\"";
    msg += ",\"appId\":\"" + json_escape(tl->app_id) + "\"";
    msg += std::string(",\"focused\":") + (tl->activated ? "true" : "false");
    msg += "}";
  }
  msg += "]}";
  send("oasis.wm", msg);
}

void FlutterHost::platform_message(const FlutterPlatformMessage* msg) {
  std::string channel = msg->channel ? msg->channel : "";
  std::string body(reinterpret_cast<const char*>(msg->message),
                   msg->message_size);
  if (channel != "oasis.wm") {
    api_.SendPlatformMessageResponse(engine_, msg->response_handle, nullptr,
                                     0);
    return;
  }
  Json j;
  std::string reply = "{}";
  if (json_parse(body, j)) {
    std::string method = j.str("method");
    int64_t id = j.integer("id");
    XdgToplevel* target = nullptr;
    for (XdgToplevel* tl : server_->toplevels)
      if (tl->id == (uint32_t)id) target = tl;
    if (method == "focus" && target) {
      server_->raise(target);
      if (target->surface()) server_->seat->set_keyboard_focus(target->surface());
    } else if (method == "close" && target) {
      xdg_toplevel_send_close(target->res);
    } else if (method == "ping") {
      reply = "{\"pong\":true}";
    }
  }
  api_.SendPlatformMessageResponse(engine_, msg->response_handle,
                                   reinterpret_cast<const uint8_t*>(reply.data()), reply.size());
}

void FlutterHost::send(const char* channel, const std::string& msg) {
  if (!engine_) return;
  FlutterPlatformMessage m = {};
  m.struct_size = sizeof(m);
  m.channel = channel;
  m.message = reinterpret_cast<const uint8_t*>(msg.data());
  m.message_size = msg.size();
  api_.SendPlatformMessage(engine_, &m);
}

void FlutterHost::log_message(const char* tag, const char* message) {
  LOGI("[dart:%s] %s", tag, message);
}

// ------------------------------------------------------------------ metrics

void FlutterHost::metrics(int32_t w, int32_t h, double ratio) {
  if (!engine_) return;
  FlutterWindowMetricsEvent ev = {};
  ev.struct_size = sizeof(ev);
  ev.width = w;
  ev.height = h;
  ev.pixel_ratio = ratio;
  ev.view_id = 0;
  api_.SendWindowMetricsEvent(engine_, &ev);
}

// ------------------------------------------------------------------ pointer

void FlutterHost::pointer(int phase, double x, double y, int64_t buttons,
                          uint64_t t_us, double scroll_x, double scroll_y) {
  if (!engine_) return;
  FlutterPointerEvent ev = {};
  ev.struct_size = sizeof(ev);
  ev.phase = (FlutterPointerPhase)phase;
  ev.timestamp = t_us;
  ev.x = x;
  ev.y = y;
  ev.device = 0;
  ev.signal_kind = (scroll_x != 0 || scroll_y != 0)
                       ? kFlutterPointerSignalKindScroll
                       : kFlutterPointerSignalKindNone;
  ev.scroll_delta_x = scroll_x;
  ev.scroll_delta_y = scroll_y;
  ev.device_kind = kFlutterPointerDeviceKindMouse;
  ev.buttons = buttons;
  ev.view_id = 0;
  api_.SendPointerEvent(engine_, &ev, 1);
}

// --------------------------------------------------------------------- key

void FlutterHost::key(uint32_t evdev, bool press, xkb_state* state,
                      xkb_keycode_t kc, uint64_t t_us) {
  if (!engine_ || !state) return;
  static std::set<uint32_t> pressed;
  xkb_keysym_t sym = xkb_state_key_get_one_sym(state, kc);
  uint32_t utf32 = press ? xkb_state_key_get_utf32(state, kc) : 0;
  uint64_t physical = hid_to_physical(evdev_to_hid(evdev));
  uint64_t logical = keysym_to_logical(sym, utf32);
  if (physical == 0 && logical == 0) return;

  char utf8[8] = {};
  if (press) xkb_state_key_get_utf8(state, kc, utf8, sizeof(utf8));

  FlutterKeyEvent ev = {};
  ev.struct_size = sizeof(ev);
  ev.timestamp = t_us;
  bool already = pressed.count(evdev) != 0;
  ev.type = press ? (already ? kFlutterKeyEventTypeRepeat
                             : kFlutterKeyEventTypeDown)
                  : kFlutterKeyEventTypeUp;
  ev.physical = physical;
  ev.logical = logical;
  ev.character = press && utf8[0] ? utf8 : nullptr;
  ev.synthesized = false;
  ev.device_type = kFlutterKeyEventDeviceTypeKeyboard;
  api_.SendKeyEvent(engine_, &ev, nullptr, nullptr);
  if (press)
    pressed.insert(evdev);
  else
    pressed.erase(evdev);
}

// -------------------------------------------------------------- task runner

void FlutterHost::post_task(FlutterTask task, uint64_t target) {
  uint64_t now = api_.GetCurrentTime();
  int ms = target > now ? (int)((target - now) / 1000000ULL) : 0;
  auto* ctx = new TaskCtx{this, task};
  wl_event_source* src = wl_event_loop_add_timer(
      loop_,
      [](void* data) -> int {
        auto* ctx = static_cast<TaskCtx*>(data);
        FlutterHost* h = ctx->host;
        FlutterTask t = ctx->task;
        delete ctx;
        h->api_.RunTask(h->engine_, &t);
        return 0;
      },
      ctx);
  wl_event_source_timer_update(src, ms);
}

// -------------------------------------------------------------------- init

bool FlutterHost::init(Server* server, const char* bundle_dir,
                       const char* engine_dir) {
  server_ = server;
  loop_ = server->loop;
  bundle_ = bundle_dir;
  enginedir_ = engine_dir;

  std::string libpath = enginedir_ + "/libflutter_engine.so";
  lib_ = dlopen(libpath.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!lib_) {
    LOGE("dlopen %s: %s", libpath.c_str(), dlerror());
    return false;
  }
  auto get_procs =
      (FlutterEngineResult(*)(FlutterEngineProcTable*))dlsym(
          lib_, "FlutterEngineGetProcAddresses");
  api_.struct_size = sizeof(api_);
  if (!get_procs || get_procs(&api_) != kSuccess) {
    LOGE("FlutterEngineGetProcAddresses failed");
    return false;
  }

  Backend* backend = server->backend;
  dpy_ = backend->egl_display();
  rctx_ = backend->new_context();
  resctx_ = backend->new_context();
  if (rctx_ == EGL_NO_CONTEXT || resctx_ == EGL_NO_CONTEXT) {
    LOGE("failed to create engine GL contexts");
    return false;
  }
  egl_create_sync_ =
      (PFNEGLCREATESYNCKHRPROC)resolve("eglCreateSyncKHR");
  egl_destroy_sync_ =
      (PFNEGLDESTROYSYNCKHRPROC)resolve("eglDestroySyncKHR");
  egl_wait_sync_ = (PFNEGLWAITSYNCKHRPROC)resolve("eglWaitSyncKHR");

  std::string assets = bundle_ + "/data/flutter_assets";
  std::string icu = enginedir_ + "/icudtl.dat";
  std::string aot = bundle_ + "/lib/libapp.so";

  FlutterProjectArgs args = {};
  args.struct_size = sizeof(args);
  args.assets_path = assets.c_str();
  args.icu_data_path = file_exists(icu) ? icu.c_str() : nullptr;
  args.shutdown_dart_vm_when_done = true;
  args.platform_message_callback = [](const FlutterPlatformMessage* m,
                                      void* ud) {
    static_cast<FlutterHost*>(ud)->platform_message(m);
  };
  args.log_message_callback = [](const char* tag, const char* msg, void* ud) {
    static_cast<FlutterHost*>(ud)->log_message(tag, msg);
  };

  main_tid_ = (pid_t)syscall(SYS_gettid);
  static FlutterTaskRunnerDescription platform_runner;
  platform_runner.struct_size = sizeof(platform_runner);
  platform_runner.user_data = this;
  platform_runner.runs_task_on_current_thread_callback = [](void* ud) {
    auto* h = static_cast<FlutterHost*>(ud);
    return (pid_t)syscall(SYS_gettid) == h->main_tid_;
  };
  platform_runner.post_task_callback = [](FlutterTask task, uint64_t target,
                                          void* ud) {
    static_cast<FlutterHost*>(ud)->post_task(task, target);
  };
  platform_runner.identifier = 1;
  static FlutterCustomTaskRunners runners = {};
  runners.struct_size = sizeof(runners);
  runners.platform_task_runner = &platform_runner;
  args.custom_task_runners = &runners;

  FlutterEngineAOTData aot_data = nullptr;
  if (file_exists(aot)) {
    FlutterEngineAOTDataSource source = {};
    source.type = kFlutterEngineAOTDataSourceTypeElfPath;
    source.elf_path = aot.c_str();
    if (api_.CreateAOTData(&source, &aot_data) == kSuccess) {
      args.aot_data = aot_data;
    } else {
      LOGE("CreateAOTData failed; will try JIT");
    }
  }
  if (!aot_data) {
    // JIT mode needs VM snapshot blobs next to the engine library
    size_t vm_size = 0, iso_size = 0;
    const uint8_t* vm =
        map_file(enginedir_ + "/vm_isolate_snapshot.bin", &vm_size);
    const uint8_t* iso =
        map_file(enginedir_ + "/isolate_snapshot.bin", &iso_size);
    args.vm_snapshot_data = vm;
    args.vm_snapshot_data_size = vm_size;
    args.isolate_snapshot_data = iso;
    args.isolate_snapshot_data_size = iso_size;
    LOGI("no AOT bundle; attempting JIT with engine snapshots");
  }

  FlutterRendererConfig cfg = {};
  cfg.type = kOpenGL;
  cfg.open_gl.struct_size = sizeof(cfg.open_gl);
  cfg.open_gl.make_current = [](void* ud) {
    return static_cast<FlutterHost*>(ud)->make_current();
  };
  cfg.open_gl.clear_current = [](void* ud) {
    return static_cast<FlutterHost*>(ud)->clear_current();
  };
  cfg.open_gl.present = [](void* ud) {
    return static_cast<FlutterHost*>(ud)->present();
  };
  cfg.open_gl.fbo_callback = [](void* ud) {
    return static_cast<FlutterHost*>(ud)->fbo();
  };
  cfg.open_gl.make_resource_current = [](void* ud) {
    return static_cast<FlutterHost*>(ud)->make_resource_current();
  };
  cfg.open_gl.fbo_reset_after_present = true;
  cfg.open_gl.gl_proc_resolver = [](void* ud, const char* name) -> void* {
    return static_cast<FlutterHost*>(ud)->resolve(name);
  };

  FlutterEngineResult res =
      api_.Run(FLUTTER_ENGINE_VERSION, &cfg, &args, this, &engine_);
  if (res != kSuccess) {
    LOGE("FlutterEngineRun failed: %d", res);
    return false;
  }
  LOGI("flutter engine running");
  return true;
}

void FlutterHost::shutdown() {
  if (engine_) api_.Shutdown(engine_);
  engine_ = nullptr;
  if (lib_) dlclose(lib_);
  lib_ = nullptr;
}

}  // namespace oasis
