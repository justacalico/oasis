#include "protocols.h"
#include "server.h"
#include "surface.h"
#include "xdg.h"
#include "xdg-decoration-unstable-v1-server.h"

namespace oasis {

// We never draw server-side decorations; every toplevel is told to use
// client-side decoration mode.
class ToplevelDecoration : public ResourceBound {
 public:
  explicit ToplevelDecoration(XdgToplevel* tl) : toplevel(tl) {}

  XdgToplevel* toplevel;

  void set_mode(wl_resource* res, uint32_t) {
    zxdg_toplevel_decoration_v1_send_configure(
        res, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
  }
  void unset_mode(wl_resource* res) {
    zxdg_toplevel_decoration_v1_send_configure(
        res, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
  }
};

static const struct zxdg_toplevel_decoration_v1_interface deco_impl = {
    destroy_request,
    REQ(ToplevelDecoration, set_mode),
    REQ(ToplevelDecoration, unset_mode),
};

static const struct zxdg_decoration_manager_v1_interface mgr_impl = {
    REQ(DecorationManager, destroy), REQ(DecorationManager, get_decoration),
};

DecorationManager::DecorationManager(Server* server) : server_(server) {
  global_.create(server->display, &zxdg_decoration_manager_v1_interface, 1,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   wl_resource* res = wl_resource_create(
                       c, &zxdg_decoration_manager_v1_interface, v, id);
                   wl_resource_set_implementation(res, &mgr_impl, this,
                                                  nullptr);
                 });
}

void DecorationManager::get_decoration(wl_client* client, wl_resource* res,
                                       uint32_t id,
                                       wl_resource* toplevel_res) {
  auto* tl = user_data<XdgToplevel>(toplevel_res);
  auto* deco = bind_resource(client, &zxdg_toplevel_decoration_v1_interface,
                             wl_resource_get_version(res), id,
                             new ToplevelDecoration(tl), &deco_impl);
  zxdg_toplevel_decoration_v1_send_configure(
      deco->res, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

}  // namespace oasis
