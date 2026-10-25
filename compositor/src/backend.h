#pragma once
#include <EGL/egl.h>
#include <wayland-server-core.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oasis {

// A display backend: produces outputs the compositor draws into and input
// events from the host system. Two implementations exist: a nested Wayland
// backend (compositor runs as a client of another compositor) and a KMS/DRM
// backend for running on real hardware.
class Backend {
 public:
  struct Output {
    std::string name;
    std::string make;
    std::string model;
    int32_t x = 0, y = 0;          // position in layout space
    int32_t width = 0, height = 0; // logical size in compositor coordinates
    int32_t phys_w = 0, phys_h = 0;
    int32_t scale = 1;
    int32_t refresh = 60000;  // mHz
    std::vector<wl_resource*> resources;
    void* backend_data = nullptr;
  };

  struct Callbacks {
    std::function<void(Output*)> frame;
    std::function<void(double, double, uint32_t)> pointer_motion;
    std::function<void(uint32_t, bool, uint32_t)> pointer_button;
    std::function<void(double, double, uint32_t)> pointer_axis;
    std::function<void(uint32_t, bool, uint32_t)> key;
    std::function<void(uint32_t, uint32_t, uint32_t, uint32_t)> modifiers;
    std::function<void(const std::string&)> keymap;
    std::function<void(Output*, int32_t, int32_t)> output_resized;
    std::function<void()> quit;
  };

  virtual ~Backend() = default;
  virtual bool init(wl_event_loop* loop, Callbacks cb) = 0;
  virtual EGLDisplay egl_display() const = 0;
  virtual EGLContext egl_context() const = 0;
  virtual EGLConfig egl_config() const = 0;
  virtual EGLContext new_context() = 0;  // share-group context, any thread
  virtual const std::vector<Output*>& outputs() const = 0;
  virtual void begin_frame(Output* o) = 0;
  virtual void end_frame(Output* o) = 0;
  virtual void schedule_frame(Output* o) {}
  virtual void set_cursor_visible(bool visible) {}
};

std::unique_ptr<Backend> create_wayland_backend();
std::unique_ptr<Backend> create_drm_backend();

}  // namespace oasis
