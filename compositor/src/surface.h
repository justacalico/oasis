#pragma once
#include <GLES2/gl2.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <memory>
#include <vector>
#include "util.h"
#include "wl.h"

namespace oasis {

class Server;
class Renderer;
class XdgSurface;
class Subsurface;

// A GPU-importable client buffer (wl_buffer) — shm or dmabuf backed.
class Buffer {
 public:
  virtual ~Buffer() = default;
  virtual GLuint texture(Renderer* r) = 0;
  virtual bool opaque() const { return false; }
  int32_t width = 0, height = 0;
  wl_resource* res = nullptr;  // wl_buffer
  bool taken = false;          // owned by a surface, outlives wl_buffer

  static Buffer* from(wl_resource* res) { return user_data<Buffer>(res); }
  // Shared wl_buffer resource destructor: frees the Buffer unless a surface
  // took ownership of it.
  static void resource_destroyed(wl_resource* res);
};

extern const struct wl_buffer_interface buffer_impl;

class Surface : public ResourceBound {
 public:
  explicit Surface(Server* server);
  ~Surface();

  Server* server;

  // committed state
  std::unique_ptr<Buffer> buffer;
  int32_t buf_dx = 0, buf_dy = 0;
  Region input_region;
  int32_t buffer_scale = 1;
  uint32_t buffer_transform = WL_OUTPUT_TRANSFORM_NORMAL;
  std::vector<wl_resource*> frame_callbacks;
  bool seq_configured = false;

  // viewport (wp_viewporter)
  bool has_viewport = false;
  double vp_src_x = -1, vp_src_y = -1, vp_src_w = -1, vp_src_h = -1;
  int32_t vp_dst_w = -1, vp_dst_h = -1;

  // subsurface state
  Subsurface* subsurface = nullptr;
  Surface* parent = nullptr;
  std::vector<Surface*> children;  // stacking order, bottom to top
  int32_t sub_x = 0, sub_y = 0;
  bool sync = false;

  struct Pending {
    bool has_buffer = false;
    wl_resource* buffer_res = nullptr;
    int32_t dx = 0, dy = 0;
    bool has_input = false;
    Region input;
    int32_t scale = 1;
    uint32_t transform = 0;
    std::vector<wl_resource*> cbs;
    bool has_viewport = false;
    double vp_src_x = -1, vp_src_y = -1, vp_src_w = -1, vp_src_h = -1;
    int32_t vp_dst_w = -1, vp_dst_h = -1;
  };
  Pending pending;
  Pending cached;
  bool has_cached = false;

  // role
  XdgSurface* xdg = nullptr;

  bool mapped() const { return buffer != nullptr; }
  int32_t width() const;
  int32_t height() const;
  void commit();
  void apply_pending(Pending& p);
  void send_frame_done(uint32_t ms);
  bool hit(double x, double y, double* sx, double* sy);
  void destroy_buffer();
  void take_buffer(wl_resource* res);
  void layout_pos(int32_t* lx, int32_t* ly);

  // wl_surface requests
  void attach(wl_resource* res, wl_resource* buffer_res, int32_t dx,
              int32_t dy);
  void damage(wl_resource*, int32_t, int32_t, int32_t, int32_t) {}
  void frame(wl_resource* res, uint32_t cb_id);
  void set_opaque_region(wl_resource*, wl_resource*) {}
  void set_input_region(wl_resource* res, wl_resource* region_res);
  void commit_request(wl_resource*) { commit(); }
  void set_buffer_transform(wl_resource* res, int32_t transform);
  void set_buffer_scale(wl_resource* res, int32_t scale);
  void damage_buffer(wl_resource*, int32_t, int32_t, int32_t, int32_t) {}
  void offset(wl_resource*, int32_t dx, int32_t dy);
};

// wl_region
class WlRegion : public ResourceBound {
 public:
  Region region;
  void add(wl_resource*, int32_t x, int32_t y, int32_t w, int32_t h) {
    region.add({x, y, w, h});
  }
  void subtract(wl_resource*, int32_t x, int32_t y, int32_t w, int32_t h) {
    region.subtract({x, y, w, h});
  }
};

// wl_subsurface: the role object linking a surface below a parent.
class Subsurface : public ResourceBound {
 public:
  Subsurface(Server* server, Surface* surface, Surface* parent);
  ~Subsurface();

  Surface* surface;

  void set_position(wl_resource*, int32_t x, int32_t y);
  void place_above(wl_resource*, wl_resource* sibling_res);
  void place_below(wl_resource*, wl_resource* sibling_res);
  void set_sync(wl_resource*);
  void set_desync(wl_resource*);

 private:
  Server* server_;
  Surface* parent_;
};

// wl_compositor
class Compositor {
 public:
  explicit Compositor(Server* server);
  Server* server;

  void create_surface(wl_client* client, wl_resource* res, uint32_t id);
  void create_region(wl_client* client, wl_resource* res, uint32_t id);

 private:
  Global global_;
};

// wl_subcompositor
class Subcompositor {
 public:
  explicit Subcompositor(Server* server);
  Server* server;

  void get_subsurface(wl_client* client, wl_resource* res, uint32_t id,
                      wl_resource* surface_res, wl_resource* parent_res);

 private:
  Global global_;
};

}  // namespace oasis
