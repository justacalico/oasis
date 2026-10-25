#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include "log.h"
#include "seat.h"
#include "server.h"
#include "surface.h"
#include "xdg.h"

namespace oasis {

using std::string;
using std::vector;

// --------------------------------------------------------------- data source

struct DataSource {
  Server* server;
  wl_resource* res;
  vector<string> mimes;
  uint32_t actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  bool is_selection = false;
  // drag state
  Surface* drag_origin = nullptr;
  Surface* drag_icon = nullptr;
  uint32_t drag_serial = 0;
  wl_client* dest_client = nullptr;
  wl_resource* dest_offer = nullptr;
  wl_resource* dest_device = nullptr;
  Surface* dest_surface = nullptr;
};

static void source_offer(wl_client*, wl_resource* res, const char* mime) {
  auto* src = static_cast<DataSource*>(wl_resource_get_user_data(res));
  src->mimes.emplace_back(mime);
}

static void source_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void source_set_actions(wl_client*, wl_resource* res,
                               uint32_t actions) {
  auto* src = static_cast<DataSource*>(wl_resource_get_user_data(res));
  src->actions = actions & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY
                   ? WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY
                   : actions;
}

static const struct wl_data_source_interface source_impl = {
    source_offer, source_destroy, source_set_actions,
};

static void source_resource_destroy(wl_resource* res);

// ---------------------------------------------------------------- data offer

struct DataOffer {
  Server* server;
  wl_resource* res;
  DataSource* source;
  bool accepted = false;
};

static void offer_accept(wl_client*, wl_resource* res, uint32_t,
                         const char* mime) {
  auto* offer = static_cast<DataOffer*>(wl_resource_get_user_data(res));
  offer->accepted = true;
  if (offer->source && offer->source->res &&
      wl_resource_get_version(offer->source->res) >=
          WL_DATA_SOURCE_TARGET_SINCE_VERSION) {
    wl_data_source_send_target(offer->source->res, mime);
  }
}

static void offer_receive(wl_client*, wl_resource* res, const char* mime,
                          int32_t fd) {
  auto* offer = static_cast<DataOffer*>(wl_resource_get_user_data(res));
  if (offer->source && offer->source->res)
    wl_data_source_send_send(offer->source->res, mime, fd);
  close(fd);
}

static void offer_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void offer_finish(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void offer_set_actions(wl_client*, wl_resource* res, uint32_t,
                              uint32_t preferred) {
  auto* offer = static_cast<DataOffer*>(wl_resource_get_user_data(res));
  if (offer->source && offer->source->res &&
      wl_resource_get_version(offer->source->res) >=
          WL_DATA_SOURCE_ACTION_SINCE_VERSION)
    wl_data_source_send_action(offer->source->res, preferred);
}

static const struct wl_data_offer_interface offer_impl = {
    offer_accept, offer_receive, offer_destroy, offer_finish,
    offer_set_actions,
};

static void offer_resource_destroy(wl_resource* res) {
  auto* offer = static_cast<DataOffer*>(wl_resource_get_user_data(res));
  if (offer->source && offer->source->dest_offer == res)
    offer->source->dest_offer = nullptr;
  delete offer;
}

static void source_resource_destroy(wl_resource* res) {
  auto* src = static_cast<DataSource*>(wl_resource_get_user_data(res));
  Server* server = src->server;
  if (server->selection_source == src) server->selection_source = nullptr;
  if (server->drag_source == src) {
    server->drag_source = nullptr;
    server->dragging = false;
  }
  if (src->dest_offer) {
    auto* offer = static_cast<DataOffer*>(
        wl_resource_get_user_data(src->dest_offer));
    if (offer) offer->source = nullptr;
    src->dest_offer = nullptr;
  }
  delete src;
}

// --------------------------------------------------------------- data device

struct DataDevice {
  Server* server;
  wl_resource* res;
};

static void device_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}
static void device_release(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void device_set_selection(wl_client* client, wl_resource* res,
                                 wl_resource* source_res, uint32_t serial) {
  auto* dev = static_cast<DataDevice*>(wl_resource_get_user_data(res));
  Server* server = dev->server;
  DataSource* src =
      source_res
          ? static_cast<DataSource*>(wl_resource_get_user_data(source_res))
          : nullptr;
  server->selection_source = src;
  server->selection_client = client;
  server->notify_selection();
}

static void device_start_drag(wl_client* client, wl_resource* res,
                              wl_resource* source_res,
                              wl_resource* origin_res, wl_resource* icon_res,
                              uint32_t serial) {
  auto* dev = static_cast<DataDevice*>(wl_resource_get_user_data(res));
  Server* server = dev->server;
  DataSource* src =
      source_res
          ? static_cast<DataSource*>(wl_resource_get_user_data(source_res))
          : nullptr;
  Surface* origin =
      static_cast<Surface*>(wl_resource_get_user_data(origin_res));
  Surface* icon =
      icon_res
          ? static_cast<Surface*>(wl_resource_get_user_data(icon_res))
          : nullptr;
  if (icon) icon->is_drag_icon = true;
  if (src) {
    src->drag_origin = origin;
    src->drag_icon = icon;
    src->drag_serial = serial;
  }
  server->drag_source = src;
  server->dragging = src != nullptr;
}

static const struct wl_data_device_interface device_impl = {
    device_start_drag, device_set_selection, device_release,
};

static void device_resource_destroy(wl_resource* res) {
  auto* dev = static_cast<DataDevice*>(wl_resource_get_user_data(res));
  auto& v = dev->server->data_devices;
  v.erase(std::remove(v.begin(), v.end(), res), v.end());
  delete dev;
}

// --------------------------------------------------------- data device mgr

static void mgr_create_source(wl_client* client, wl_resource* res,
                              uint32_t id) {
  auto* server = static_cast<Server*>(wl_resource_get_user_data(res));
  wl_resource* sres =
      wl_resource_create(client, &wl_data_source_interface,
                         wl_resource_get_version(res), id);
  auto* src = new DataSource();
  src->server = server;
  src->res = sres;
  wl_resource_set_implementation(sres, &source_impl, src,
                                 source_resource_destroy);
}

static void mgr_get_data_device(wl_client* client, wl_resource* res,
                                uint32_t id, wl_resource* seat_res) {
  auto* server = static_cast<Server*>(wl_resource_get_user_data(res));
  wl_resource* dres =
      wl_resource_create(client, &wl_data_device_interface,
                         wl_resource_get_version(res), id);
  auto* dev = new DataDevice{server, dres};
  server->data_devices.push_back(dres);
  wl_resource_set_implementation(dres, &device_impl, dev,
                                 device_resource_destroy);
}

static const struct wl_data_device_manager_interface mgr_impl = {
    mgr_create_source, mgr_get_data_device,
};

static void mgr_bind(wl_client* client, void* data, uint32_t version,
                   uint32_t id) {
  auto* server = static_cast<Server*>(data);
  version = std::min<uint32_t>(version, 3);
  wl_resource* res =
      wl_resource_create(client, &wl_data_device_manager_interface, version,
                         id);
  wl_resource_set_implementation(res, &mgr_impl, server, nullptr);
}

void data_device_global_init(Server* server) {
  wl_global_create(server->display, &wl_data_device_manager_interface, 3,
                   server, mgr_bind);
}

// ----------------------------------------------------------- server helpers

// Sends a selection offer to the currently focused client. Called when the
// selection changes or focus moves.
void Server::notify_selection() {
  if (!seat || !seat->keyboard_focus) return;
  wl_client* fc = wl_resource_get_client(seat->keyboard_focus->res);
  for (wl_resource* dev : data_devices) {
    if (wl_resource_get_client(dev) != fc) continue;
    if (!selection_source) {
      wl_data_device_send_selection(dev, nullptr);
      continue;
    }
    auto* src = static_cast<DataSource*>(selection_source);
    wl_resource* ores = wl_resource_create(
        fc, &wl_data_offer_interface, wl_resource_get_version(dev), 0);
    auto* offer = new DataOffer{this, ores, src};
    wl_resource_set_implementation(ores, &offer_impl, offer,
                                   offer_resource_destroy);
    wl_data_device_send_data_offer(dev, ores);
    for (auto& m : src->mimes) wl_data_offer_send_offer(ores, m.c_str());
    wl_data_device_send_selection(dev, ores);
  }
}

// Routes drag-and-drop pointer activity. Called from Seat on motion/button.
void Server::dnd_motion(double x, double y, uint32_t t) {
  if (!dragging || !drag_source) return;
  Surface* s = nullptr;
  double sx = 0, sy = 0;
  surface_at(x, y, &s, &sx, &sy);
  DataSource* src = static_cast<DataSource*>(drag_source);
  if (s == drag_target_surface) {
    if (src->dest_device)
      wl_data_device_send_motion(src->dest_device, t,
                                wl_fixed_from_double(sx),
                                wl_fixed_from_double(sy));
    return;
  }
  // left previous target
  if (src->dest_device)
    wl_data_device_send_leave(src->dest_device);
  src->dest_offer = nullptr;
  src->dest_client = nullptr;
  src->dest_device = nullptr;
  drag_target_surface = s;
  if (!s) return;
  wl_client* c = wl_resource_get_client(s->res);
  for (wl_resource* dev : data_devices) {
    if (wl_resource_get_client(dev) != c) continue;
    wl_resource* ores = wl_resource_create(
        c, &wl_data_offer_interface, wl_resource_get_version(dev), 0);
    auto* offer = new DataOffer{this, ores, src};
    wl_resource_set_implementation(ores, &offer_impl, offer,
                                   offer_resource_destroy);
    src->dest_offer = ores;
    src->dest_client = c;
    src->dest_device = dev;
    for (auto& m : src->mimes) wl_data_offer_send_offer(ores, m.c_str());
    if (wl_resource_get_version(ores) >=
        WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION) {
      wl_data_offer_send_source_actions(ores, src->actions);
      wl_data_offer_send_action(ores, src->actions);
    }
    wl_data_device_send_enter(dev, wl_display_next_serial(display), s->res,
                              wl_fixed_from_double(sx),
                              wl_fixed_from_double(sy), ores);
    return;
  }
}

void Server::dnd_drop(uint32_t t) {
  if (!dragging || !drag_source) return;
  DataSource* src = static_cast<DataSource*>(drag_source);
  if (src->dest_device) {
    wl_data_device_send_drop(src->dest_device);
  }
  if (src->res &&
      wl_resource_get_version(src->res) >=
          WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION)
    wl_data_source_send_dnd_finished(src->res);
  src->dest_offer = nullptr;
  src->dest_device = nullptr;
  dragging = false;
  drag_source = nullptr;
  drag_target_surface = nullptr;
}

}  // namespace oasis
