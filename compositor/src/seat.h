#pragma once
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <cstdint>
#include <string>
#include <vector>

namespace oasis {

class Server;
class Surface;
class XdgToplevel;
class XdgPopup;

class Seat {
 public:
  explicit Seat(Server* server);
  ~Seat();

  bool init();

  Server* server;
  wl_global* global = nullptr;

  std::vector<wl_resource*> pointers;
  std::vector<wl_resource*> keyboards;

  Surface* pointer_focus = nullptr;
  double focus_sx = 0, focus_sy = 0;
  Surface* keyboard_focus = nullptr;
  int64_t flutter_buttons = 0;
  bool flutter_added = false;

  // xkb state for translation and for wl_keyboard clients
  xkb_context* xkb_ctx = nullptr;
  xkb_keymap* keymap = nullptr;
  xkb_state* kbd_state = nullptr;
  std::string keymap_str;
  uint32_t mods_dep = 0, mods_lat = 0, mods_lock = 0, group = 0;

  enum class Grab { None, Move, Resize } grab = Grab::None;
  XdgToplevel* grab_toplevel = nullptr;
  uint32_t grab_edges = 0;
  double grab_x = 0, grab_y = 0;  // pointer offset in toplevel
  std::vector<XdgPopup*> popup_stack;  // popups holding the grab

  void set_keymap(const std::string& str);
  void send_keymap(wl_resource* kbd);
  void send_modifiers();

  void motion(double x, double y, uint32_t t);
  void button(uint32_t button, bool pressed, uint32_t t);
  void axis(double dx, double dy, uint32_t t);
  void key(uint32_t evdev, bool pressed, uint32_t t);
  void modifiers(uint32_t dep, uint32_t lat, uint32_t lock, uint32_t grp);

  void set_pointer_focus(Surface* s, double sx, double sy);
  void set_keyboard_focus(Surface* s);
  void flutter_pointer(int phase_extra, double x, double y, uint32_t t,
                       double sx = 0, double sy = 0);

  void begin_move(XdgToplevel* t);
  void begin_resize(XdgToplevel* t, uint32_t edges);
  void popup_grab(XdgPopup* p);
  bool pointer_grabbed() const {
    return grab != Grab::None || !popup_stack.empty();
  }
  void release_popups_from(XdgPopup* p);
};

}  // namespace oasis
