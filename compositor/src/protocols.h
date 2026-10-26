#pragma once
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <wayland-server-core.h>
#include <vector>
#include "backend.h"
#include "util.h"
#include "wl.h"

namespace oasis {

class Server;
class Surface;
class DataSource;

// wl_shm: shared-memory buffer pools.
class Shm {
 public:
  explicit Shm(Server* server);
  void create_pool(wl_client* client, wl_resource* res, uint32_t id,
                   int32_t fd, int32_t size);

 private:
  Server* server_;
  Global global_;
};

// zwp_linux_dmabuf_v1: dmabuf import into GL textures via EGL images.
class Dmabuf {
 public:
  explicit Dmabuf(Server* server);
  bool available() const { return create_image_ != nullptr; }

  void create_params(wl_client* client, wl_resource* res, uint32_t id);
  void bind(wl_client* client, uint32_t version, uint32_t id);

  EGLImage create_image(const EGLint* attrs) const;
  void destroy_image(EGLImage image) const;
  void image_to_texture(GLuint tex, EGLImage image) const;

 private:
  Global global_;
  EGLDisplay dpy_ = EGL_NO_DISPLAY;
  PFNEGLCREATEIMAGEKHRPROC create_image_ = nullptr;
  PFNEGLDESTROYIMAGEKHRPROC destroy_image_ = nullptr;
  PFNEGLQUERYDMABUFFORMATSEXTPROC query_formats_ = nullptr;
  PFNEGLQUERYDMABUFMODIFIERSEXTPROC query_modifiers_ = nullptr;
  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC image_target_ = nullptr;
};

// wl_data_device_manager: clipboard and drag-and-drop between clients.
class DataDeviceManager {
 public:
  explicit DataDeviceManager(Server* server);

  void create_source(wl_client* client, wl_resource* res, uint32_t id);
  void get_data_device(wl_client* client, wl_resource* res, uint32_t id,
                       wl_resource* seat_res);

  bool dragging() const { return drag_source_ != nullptr; }
  void set_selection(DataSource* src);
  void begin_drag(DataSource* src, Surface* origin, Surface* icon,
                  uint32_t serial);
  void remove_device(wl_resource* res);
  void notify_selection();
  void dnd_motion(double x, double y, uint32_t t);
  void dnd_drop(uint32_t t);
  void clear_source(DataSource* src);
  void clear_drag_target(Surface* s);

 private:
  Server* server_;
  Global global_;
  std::vector<wl_resource*> devices_;
  DataSource* selection_ = nullptr;
  DataSource* drag_source_ = nullptr;
  Surface* drag_target_ = nullptr;
};

// zxdg_decoration_manager_v1: forces client-side decorations.
class DecorationManager {
 public:
  explicit DecorationManager(Server* server);
  void destroy(wl_resource* res) { wl_resource_destroy(res); }
  void get_decoration(wl_client* client, wl_resource* res, uint32_t id,
                      wl_resource* toplevel_res);

 private:
  Server* server_;
  Global global_;
};

// wp_viewporter: crop/scale surface content.
class Viewporter {
 public:
  explicit Viewporter(Server* server);
  void destroy(wl_resource* res) { wl_resource_destroy(res); }
  void get_viewport(wl_client* client, wl_resource* res, uint32_t id,
                    wl_resource* surface_res);

 private:
  Server* server_;
  Global global_;
};

// wl_output advertised per backend output.
class OutputGlobal {
 public:
  OutputGlobal(Server* server, Backend::Output* out);
  Backend::Output* output() const { return out_; }
  void send_mode();

 private:
  void bind(wl_client* client, uint32_t version, uint32_t id);
  static void resource_destroyed(wl_resource* res);
  Backend::Output* out_;
  Global global_;
};

}  // namespace oasis
