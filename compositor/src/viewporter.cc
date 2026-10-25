#include "server.h"
#include "surface.h"
#include "viewporter-server.h"

namespace oasis {

struct Viewport {
  Surface* surface;
};

static void vp_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void vp_set_source(wl_client*, wl_resource* res, wl_fixed_t x,
                          wl_fixed_t y, wl_fixed_t w, wl_fixed_t h) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  s->pending.has_viewport = true;
  s->pending.vp_src_x = wl_fixed_to_double(x);
  s->pending.vp_src_y = wl_fixed_to_double(y);
  s->pending.vp_src_w = wl_fixed_to_double(w);
  s->pending.vp_src_h = wl_fixed_to_double(h);
}

static void vp_set_destination(wl_client*, wl_resource* res, int32_t w,
                               int32_t h) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(res));
  s->pending.has_viewport = true;
  s->pending.vp_dst_w = w;
  s->pending.vp_dst_h = h;
}

static const struct wp_viewport_interface vp_impl = {
    vp_destroy, vp_set_source, vp_set_destination,
};

static void viewporter_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void viewporter_get_viewport(wl_client* client, wl_resource* res,
                                    uint32_t id, wl_resource* surface_res) {
  auto* s = static_cast<Surface*>(wl_resource_get_user_data(surface_res));
  wl_resource* vres =
      wl_resource_create(client, &wp_viewport_interface,
                         wl_resource_get_version(res), id);
  wl_resource_set_implementation(vres, &vp_impl, s, nullptr);
}

static const struct wp_viewporter_interface viewporter_impl = {
    viewporter_destroy, viewporter_get_viewport,
};

static void viewporter_bind(wl_client* client, void* data, uint32_t version,
                            uint32_t id) {
  wl_resource* res =
      wl_resource_create(client, &wp_viewporter_interface, version, id);
  wl_resource_set_implementation(res, &viewporter_impl, data, nullptr);
}

void viewporter_global_init(Server* server) {
  wl_global_create(server->display, &wp_viewporter_interface, 1, server,
                   viewporter_bind);
}

}  // namespace oasis
