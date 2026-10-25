#include "wl.h"

#include <wayland-server-protocol.h>

namespace oasis {

void destroy_request(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

extern const struct wl_buffer_interface buffer_impl = {destroy_request};

}  // namespace oasis
