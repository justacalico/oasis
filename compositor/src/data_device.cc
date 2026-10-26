#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include "log.h"
#include "protocols.h"
#include "seat.h"
#include "server.h"
#include "surface.h"
#include "xdg.h"

namespace oasis {

// --------------------------------------------------------------- data source

class DataSource : public ResourceBound {
 public:
  explicit DataSource(DataDeviceManager* mgr) : mgr(mgr) {}
  ~DataSource();

  DataDeviceManager* mgr;
  std::vector<std::string> mimes;
  uint32_t actions = WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY;
  // drag state
  Surface* drag_origin = nullptr;
  Surface* drag_icon = nullptr;
  uint32_t drag_serial = 0;
  wl_resource* dest_offer = nullptr;
  wl_resource* dest_device = nullptr;

  void offer(wl_resource*, const char* mime) { mimes.emplace_back(mime); }
  void set_actions(wl_resource*, uint32_t a) {
    actions = a & WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY
                  ? WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY
                  : a;
  }
};

// ----------------------------------------------------------------- data offer

class DataOffer : public ResourceBound {
 public:
  explicit DataOffer(DataSource* src) : source(src) {}
  ~DataOffer() {
    if (source && source->dest_offer == res) source->dest_offer = nullptr;
  }

  DataSource* source;
  bool accepted = false;

  void accept(wl_resource*, uint32_t, const char* mime) {
    accepted = true;
    if (source && source->res &&
        wl_resource_get_version(source->res) >=
            WL_DATA_SOURCE_TARGET_SINCE_VERSION)
      wl_data_source_send_target(source->res, mime);
  }
  void receive(wl_resource*, const char* mime, int32_t fd) {
    if (source && source->res)
      wl_data_source_send_send(source->res, mime, fd);
    close(fd);
  }
  void set_actions(wl_resource*, uint32_t, uint32_t preferred) {
    if (source && source->res &&
        wl_resource_get_version(source->res) >=
            WL_DATA_SOURCE_ACTION_SINCE_VERSION)
      wl_data_source_send_action(source->res, preferred);
  }
};

// --------------------------------------------------------------- data device

class DataDevice : public ResourceBound {
 public:
  explicit DataDevice(DataDeviceManager* mgr) : mgr(mgr) {}
  ~DataDevice() { mgr->remove_device(res); }

  DataDeviceManager* mgr;

  void start_drag(wl_client* client, wl_resource* res,
                  wl_resource* source_res, wl_resource* origin_res,
                  wl_resource* icon_res, uint32_t serial);
  void set_selection(wl_client* client, wl_resource* res,
                     wl_resource* source_res, uint32_t serial);
};

static const struct wl_data_source_interface source_impl = {
    REQ(DataSource, offer), destroy_request, REQ(DataSource, set_actions),
};

static const struct wl_data_offer_interface offer_impl = {
    REQ(DataOffer, accept), REQ(DataOffer, receive), destroy_request,
    destroy_request,        REQ(DataOffer, set_actions),
};

static const struct wl_data_device_interface device_impl = {
    REQ(DataDevice, start_drag), REQ(DataDevice, set_selection),
    destroy_request,
};

static const struct wl_data_device_manager_interface mgr_impl = {
    REQ(DataDeviceManager, create_source),
    REQ(DataDeviceManager, get_data_device),
};

DataSource::~DataSource() {
  mgr->clear_source(this);
  if (dest_offer) {
    auto* o = user_data<DataOffer>(dest_offer);
    if (o) o->source = nullptr;
  }
}

// --------------------------------------------------------- data device mgr

DataDeviceManager::DataDeviceManager(Server* server) : server_(server) {
  global_.create(server->display, &wl_data_device_manager_interface, 3,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   v = std::min<uint32_t>(v, 3);
                   wl_resource* res = wl_resource_create(
                       c, &wl_data_device_manager_interface, v, id);
                   wl_resource_set_implementation(res, &mgr_impl, this,
                                                  nullptr);
                 });
}

void DataDeviceManager::create_source(wl_client* client, wl_resource* res,
                                      uint32_t id) {
  bind_resource(client, &wl_data_source_interface,
                wl_resource_get_version(res), id, new DataSource(this),
                &source_impl);
}

void DataDeviceManager::get_data_device(wl_client* client, wl_resource* res,
                                        uint32_t id,
                                        wl_resource* seat_res) {
  auto* dev = bind_resource(client, &wl_data_device_interface,
                            wl_resource_get_version(res), id,
                            new DataDevice(this), &device_impl);
  devices_.push_back(dev->res);
}

void DataDeviceManager::remove_device(wl_resource* res) {
  devices_.erase(std::remove(devices_.begin(), devices_.end(), res),
                 devices_.end());
}

void DataDevice::set_selection(wl_client* client, wl_resource*,
                               wl_resource* source_res, uint32_t serial) {
  mgr->set_selection(source_res ? user_data<DataSource>(source_res)
                                : nullptr);
}

void DataDevice::start_drag(wl_client* client, wl_resource*,
                            wl_resource* source_res, wl_resource* origin_res,
                            wl_resource* icon_res, uint32_t serial) {
  mgr->begin_drag(
      source_res ? user_data<DataSource>(source_res) : nullptr,
      user_data<Surface>(origin_res),
      icon_res ? user_data<Surface>(icon_res) : nullptr, serial);
}

void DataDeviceManager::set_selection(DataSource* src) {
  selection_ = src;
  notify_selection();
}

void DataDeviceManager::begin_drag(DataSource* src, Surface* origin,
                                   Surface* icon, uint32_t serial) {
  if (src) {
    src->drag_origin = origin;
    src->drag_icon = icon;
    src->drag_serial = serial;
  }
  drag_source_ = src;
}

void DataDeviceManager::clear_source(DataSource* src) {
  if (selection_ == src) selection_ = nullptr;
  if (drag_source_ == src) drag_source_ = nullptr;
}

void DataDeviceManager::clear_drag_target(Surface* s) {
  if (drag_target_ == s) drag_target_ = nullptr;
}

// Sends a selection offer to the currently focused client. Called when the
// selection changes or focus moves.
void DataDeviceManager::notify_selection() {
  if (!server_->seat || !server_->seat->keyboard_focus) return;
  wl_client* fc = wl_resource_get_client(server_->seat->keyboard_focus->res);
  for (wl_resource* dev : devices_) {
    if (wl_resource_get_client(dev) != fc) continue;
    if (!selection_) {
      wl_data_device_send_selection(dev, nullptr);
      continue;
    }
    wl_resource* ores = wl_resource_create(
        fc, &wl_data_offer_interface, wl_resource_get_version(dev), 0);
    attach_resource(ores, new DataOffer(selection_), &offer_impl);
    wl_data_device_send_data_offer(dev, ores);
    for (auto& m : selection_->mimes) wl_data_offer_send_offer(ores, m.c_str());
    wl_data_device_send_selection(dev, ores);
  }
}

// Routes drag-and-drop pointer activity. Called from the server on motion
// and button events while a drag is active.
void DataDeviceManager::dnd_motion(double x, double y, uint32_t t) {
  if (!drag_source_) return;
  Surface* s = nullptr;
  double sx = 0, sy = 0;
  server_->surface_at(x, y, &s, &sx, &sy);
  DataSource* src = drag_source_;
  if (s == drag_target_) {
    if (src->dest_device)
      wl_data_device_send_motion(src->dest_device, t,
                                 wl_fixed_from_double(sx),
                                 wl_fixed_from_double(sy));
    return;
  }
  // left previous target
  if (src->dest_device) wl_data_device_send_leave(src->dest_device);
  src->dest_offer = nullptr;
  src->dest_device = nullptr;
  drag_target_ = s;
  if (!s) return;
  wl_client* c = wl_resource_get_client(s->res);
  for (wl_resource* dev : devices_) {
    if (wl_resource_get_client(dev) != c) continue;
    wl_resource* ores = wl_resource_create(
        c, &wl_data_offer_interface, wl_resource_get_version(dev), 0);
    attach_resource(ores, new DataOffer(src), &offer_impl);
    src->dest_offer = ores;
    src->dest_device = dev;
    for (auto& m : src->mimes) wl_data_offer_send_offer(ores, m.c_str());
    if (wl_resource_get_version(ores) >=
        WL_DATA_OFFER_SOURCE_ACTIONS_SINCE_VERSION) {
      wl_data_offer_send_source_actions(ores, src->actions);
      wl_data_offer_send_action(ores, src->actions);
    }
    wl_data_device_send_enter(dev, wl_display_next_serial(server_->display),
                              s->res, wl_fixed_from_double(sx),
                              wl_fixed_from_double(sy), ores);
    return;
  }
}

void DataDeviceManager::dnd_drop(uint32_t t) {
  if (!drag_source_) return;
  DataSource* src = drag_source_;
  if (src->dest_device) wl_data_device_send_drop(src->dest_device);
  if (src->res && wl_resource_get_version(src->res) >=
                      WL_DATA_SOURCE_DND_FINISHED_SINCE_VERSION)
    wl_data_source_send_dnd_finished(src->res);
  src->dest_offer = nullptr;
  src->dest_device = nullptr;
  drag_source_ = nullptr;
  drag_target_ = nullptr;
}

}  // namespace oasis
