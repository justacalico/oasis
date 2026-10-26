#include "protocols.h"
#include "server.h"
#include "surface.h"
#include "viewporter-server.h"

namespace oasis {

// wp_viewport: applies pending crop/scale state to its surface on commit.
class Viewport : public ResourceBound {
 public:
  explicit Viewport(Surface* s) : surface(s) {}

  Surface* surface;

  void set_source(wl_resource*, wl_fixed_t x, wl_fixed_t y, wl_fixed_t w,
                  wl_fixed_t h) {
    surface->pending.has_viewport = true;
    surface->pending.vp_src_x = wl_fixed_to_double(x);
    surface->pending.vp_src_y = wl_fixed_to_double(y);
    surface->pending.vp_src_w = wl_fixed_to_double(w);
    surface->pending.vp_src_h = wl_fixed_to_double(h);
  }
  void set_destination(wl_resource*, int32_t w, int32_t h) {
    surface->pending.has_viewport = true;
    surface->pending.vp_dst_w = w;
    surface->pending.vp_dst_h = h;
  }
};

static const struct wp_viewport_interface viewport_impl = {
    destroy_request,
    REQ(Viewport, set_source),
    REQ(Viewport, set_destination),
};

static const struct wp_viewporter_interface viewporter_impl = {
    REQ(Viewporter, destroy), REQ(Viewporter, get_viewport),
};

Viewporter::Viewporter(Server* server) : server_(server) {
  global_.create(server->display, &wp_viewporter_interface, 1,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   wl_resource* res = wl_resource_create(
                       c, &wp_viewporter_interface, v, id);
                   wl_resource_set_implementation(res, &viewporter_impl, this,
                                                  nullptr);
                 });
}

void Viewporter::get_viewport(wl_client* client, wl_resource* res, uint32_t id,
                              wl_resource* surface_res) {
  bind_resource(client, &wp_viewport_interface, wl_resource_get_version(res),
                id, new Viewport(user_data<Surface>(surface_res)),
                &viewport_impl);
}

}  // namespace oasis
