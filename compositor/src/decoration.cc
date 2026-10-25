#include "server.h"
#include "surface.h"
#include "xdg.h"
#include "xdg-decoration-unstable-v1-server.h"

namespace oasis {

// We never draw server-side decorations; every toplevel is told to use
// client-side decoration mode.
struct ToplevelDecoration {
  wl_resource* res;
  XdgToplevel* tl;
};

static void deco_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void deco_set_mode(wl_client*, wl_resource* res, uint32_t) {
  zxdg_toplevel_decoration_v1_send_configure(
      res, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static void deco_unset_mode(wl_client*, wl_resource* res) {
  zxdg_toplevel_decoration_v1_send_configure(
      res, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static const struct zxdg_toplevel_decoration_v1_interface deco_impl = {
    deco_destroy, deco_set_mode, deco_unset_mode,
};

static void deco_mgr_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void deco_mgr_get(wl_client* client, wl_resource* res, uint32_t id,
                         wl_resource* toplevel_res) {
  auto* tl = static_cast<XdgToplevel*>(
      wl_resource_get_user_data(toplevel_res));
  wl_resource* dres =
      wl_resource_create(client, &zxdg_toplevel_decoration_v1_interface,
                         wl_resource_get_version(res), id);
  auto* deco = new ToplevelDecoration{dres, tl};
  wl_resource_set_implementation(dres, &deco_impl, deco,
                                 [](wl_resource* r) {
                                   delete static_cast<ToplevelDecoration*>(
                                       wl_resource_get_user_data(r));
                                 });
  zxdg_toplevel_decoration_v1_send_configure(
      dres, ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
}

static const struct zxdg_decoration_manager_v1_interface deco_mgr_impl = {
    deco_mgr_destroy, deco_mgr_get,
};

static void deco_mgr_bind(wl_client* client, void* data, uint32_t version,
                          uint32_t id) {
  wl_resource* res =
      wl_resource_create(client, &zxdg_decoration_manager_v1_interface,
                         version, id);
  wl_resource_set_implementation(res, &deco_mgr_impl, data, nullptr);
}

void decoration_global_init(Server* server) {
  wl_global_create(server->display, &zxdg_decoration_manager_v1_interface, 1,
                   server, deco_mgr_bind);
}

}  // namespace oasis
