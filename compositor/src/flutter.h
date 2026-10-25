#pragma once
#include <EGL/egl.h>
#include <wayland-server-core.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <xkbcommon/xkbcommon.h>
#include <cstdint>
#include <set>
#include <string>
#include <vector>
#include "flutter_embedder.h"

namespace oasis {

class Server;
class XdgToplevel;

// Hosts the Flutter engine inside the compositor: the shell UI renders into
// an FBO we composite as the bottom-most layer. Communication with Dart uses
// a string-codec platform channel carrying JSON.
class FlutterHost {
 public:
  ~FlutterHost() { shutdown(); }

  bool init(Server* server, const char* bundle_dir, const char* engine_dir);
  void shutdown();
  bool running() const { return engine_ != nullptr; }

  void metrics(int32_t w, int32_t h, double ratio);
  void pointer(int phase, double x, double y, int64_t buttons, uint64_t t_us,
               double scroll_x = 0, double scroll_y = 0);
  void key(uint32_t evdev, bool press, xkb_state* state, xkb_keycode_t kc,
           uint64_t t_us);
  void notify_windows();
  void send(const char* channel, const std::string& msg);

  // Called by the compositor to composite the shell. Returns the texture to
  // draw or 0 if none has been presented yet.
  GLuint shell_texture();

  // engine -> us
  bool make_current();
  bool clear_current();
  bool present();
  uint32_t fbo();
  bool make_resource_current();
  void* resolve(const char* name);
  void platform_message(const FlutterPlatformMessage* msg);
  void log_message(const char* tag, const char* message);
  void post_task(FlutterTask task, uint64_t target);

 private:
  Server* server_ = nullptr;
  void* lib_ = nullptr;
  FLUTTER_API_SYMBOL(FlutterEngine) engine_ = nullptr;
  FlutterEngineProcTable api_{};
  pid_t main_tid_ = 0;
  wl_event_loop* loop_ = nullptr;
  FlutterTaskRunnerDescription platform_runner_{};
  FlutterCustomTaskRunners runners_{};

  EGLDisplay dpy_ = EGL_NO_DISPLAY;
  EGLContext rctx_ = EGL_NO_CONTEXT;   // engine render thread
  EGLContext resctx_ = EGL_NO_CONTEXT; // engine resource thread
  PFNEGLCREATESYNCKHRPROC egl_create_sync_ = nullptr;
  PFNEGLDESTROYSYNCKHRPROC egl_destroy_sync_ = nullptr;
  PFNEGLWAITSYNCKHRPROC egl_wait_sync_ = nullptr;

  // owned on the render thread; ping-ponged so the presented texture is
  // never overwritten while the main thread samples it
  GLuint fbos_[2] = {0, 0}, fbo_texs_[2] = {0, 0};
  GLuint fbo_ = 0;
  int fbo_index_ = 0;
  int32_t fbo_w_ = 0, fbo_h_ = 0;
  // handed to the main thread at present()
  GLuint front_tex_ = 0;
  int32_t front_w_ = 0, front_h_ = 0;
  EGLSyncKHR fence_ = EGL_NO_SYNC_KHR;
  bool have_frame_ = false;

  std::set<uint32_t> pressed_keys_;
  std::string bundle_, enginedir_;
  std::vector<uint8_t> jit_vm_, jit_iso_;
};

}  // namespace oasis
