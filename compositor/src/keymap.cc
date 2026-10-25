#include "keymap.h"

namespace oasis {

// evdev keycode -> USB HID usage, from the kernel's usb_kbd_keycode table
// (drivers/hid/usbhid). Index is the evdev code, value the HID usage.
static const uint8_t kEvdevToHid[256] = {
    0,   0,   0,   0,   30,  48,  46,  32,  18,  33,  34,  35,  23,  36,
    37,  38,  50,  49,  24,  25,  16,  19,  31,  20,  22,  47,  17,  45,
    21,  44,  2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  28,  1,
    14,  15,  57,  12,  13,  26,  27,  43,  43,  39,  40,  41,  51,  52,
    53,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  87,  88,
    99,  70,  119, 110, 102, 104, 111, 107, 109, 106, 105, 108, 103, 69,
    98,  55,  74,  78,  96,  79,  80,  81,  75,  76,  77,  71,  72,  73,
    82,  83,  86,  127, 116, 117, 183, 184, 185, 186, 187, 188, 189, 190,
    191, 192, 193, 194, 134, 138, 130, 132, 128, 129, 131, 137, 133, 135,
    136, 113, 115, 114, 0,   0,   0,   121, 0,   89,  93,  124, 92,  94,
    95,  0,   0,   0,   122, 123, 90,  91,  85,  0,   0,   0,   0,   0,
    0,   0,   111, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   111, 0,   0,   0,   0,   0,
    0,   0,   0,   0,   29,  42,  56,  125, 97,  54,  100, 126, 164, 166,
    165, 163, 161, 115, 114, 113, 150, 158, 159, 128, 136, 177, 178, 176,
    142, 152, 173, 140,
};

uint32_t evdev_to_hid(uint32_t evdev) {
  if (evdev >= 256) return 0;
  return kEvdevToHid[evdev];
}

uint64_t hid_to_physical(uint32_t hid) {
  return hid ? 0x00070000ull | hid : 0;
}

// keysym -> Flutter logical key id. Printable characters use their Unicode
// code point (handled by the utf32 fallback); this table covers named keys.
uint64_t keysym_to_logical(xkb_keysym_t sym, uint32_t utf32) {
  if (utf32 >= 0x20 && utf32 != 0x7f) return utf32;
  switch (sym) {
    case XKB_KEY_BackSpace: return 0x00100000008;
    case XKB_KEY_Tab:       return 0x00100000009;
    case XKB_KEY_Return:    return 0x0010000000d;
    case XKB_KEY_KP_Enter:  return 0x0010000000d;
    case XKB_KEY_Escape:    return 0x0010000001b;
    case XKB_KEY_Delete:    return 0x0010000007f;
    case XKB_KEY_Caps_Lock: return 0x00100000104;
    case XKB_KEY_Num_Lock:  return 0x0010000010a;
    case XKB_KEY_Scroll_Lock: return 0x0010000010c;
    case XKB_KEY_Down:      return 0x00100000301;
    case XKB_KEY_Left:      return 0x00100000302;
    case XKB_KEY_Right:     return 0x00100000303;
    case XKB_KEY_Up:        return 0x00100000304;
    case XKB_KEY_End:       return 0x00100000305;
    case XKB_KEY_Home:      return 0x00100000306;
    case XKB_KEY_Page_Down: return 0x00100000307;
    case XKB_KEY_Page_Up:   return 0x00100000308;
    case XKB_KEY_Insert:    return 0x00100000407;
    case XKB_KEY_Menu:      return 0x00100000505;
    case XKB_KEY_Pause:     return 0x00100000509;
    case XKB_KEY_Print:     return 0x00100000608;
    case XKB_KEY_F1:  return 0x00100000801;
    case XKB_KEY_F2:  return 0x00100000802;
    case XKB_KEY_F3:  return 0x00100000803;
    case XKB_KEY_F4:  return 0x00100000804;
    case XKB_KEY_F5:  return 0x00100000805;
    case XKB_KEY_F6:  return 0x00100000806;
    case XKB_KEY_F7:  return 0x00100000807;
    case XKB_KEY_F8:  return 0x00100000808;
    case XKB_KEY_F9:  return 0x00100000809;
    case XKB_KEY_F10: return 0x0010000080a;
    case XKB_KEY_F11: return 0x0010000080b;
    case XKB_KEY_F12: return 0x0010000080c;
    case XKB_KEY_F13: return 0x0010000080d;
    case XKB_KEY_F14: return 0x0010000080e;
    case XKB_KEY_F15: return 0x0010000080f;
    case XKB_KEY_F16: return 0x00100000810;
    case XKB_KEY_F17: return 0x00100000811;
    case XKB_KEY_F18: return 0x00100000812;
    case XKB_KEY_F19: return 0x00100000813;
    case XKB_KEY_F20: return 0x00100000814;
    case XKB_KEY_F21: return 0x00100000815;
    case XKB_KEY_F22: return 0x00100000816;
    case XKB_KEY_F23: return 0x00100000817;
    case XKB_KEY_F24: return 0x00100000818;
    case XKB_KEY_Control_L:  return 0x00200000100;
    case XKB_KEY_Control_R:  return 0x00200000101;
    case XKB_KEY_Shift_L:    return 0x00200000102;
    case XKB_KEY_Shift_R:    return 0x00200000103;
    case XKB_KEY_Alt_L:      return 0x00200000104;
    case XKB_KEY_Alt_R:      return 0x00200000105;
    case XKB_KEY_Meta_L:     return 0x00200000106;
    case XKB_KEY_Meta_R:     return 0x00200000107;
    case XKB_KEY_Super_L:    return 0x00200000106;
    case XKB_KEY_Super_R:    return 0x00200000107;
    default: return 0;
  }
}

}  // namespace oasis
