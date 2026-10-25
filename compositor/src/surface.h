#pragma once
#include <GLES2/gl2.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <memory>
#include <vector>
#include "util.h"

namespace oasis {

class Server;
class Renderer;
class XdgSurface;

// A GPU-importable client buffer (wl_buffer) — shm or dmabuf backed.
class Buffer {
 public:
  virtual ~Buffer() = default;
  virtual GLuint texture(Renderer* r) = 0;
  virtual bool opaque() const { return false; }
  int32_t width = 0, height = 0;
  wl_resource* res = nullptr;  // wl_buffer
  bool taken = false;          // owned by a surface, outlives wl_buffer
};

class Surface {
 public:
  Surface(Server* server, wl_resource* res);
  ~Surface();

  Server* server;
  wl_resource* res;

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
  wl_resource* subsurface_res = nullptr;
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
  bool is_popup = false;
  bool is_drag_icon = false;

  bool mapped() const { return buffer != nullptr; }
  int32_t width() const;
  int32_t height() const;
  void commit();
  void apply_pending(Pending& p);
  void send_frame_done(uint32_t ms);
  bool hit(double x, double y, double* sx, double* sy);
  void destroy_buffer();
};

}  // namespace oasis
