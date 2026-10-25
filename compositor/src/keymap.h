#pragma once
#include <xkbcommon/xkbcommon.h>
#include <cstdint>
#include <string>

namespace oasis {

uint32_t evdev_to_hid(uint32_t evdev);
uint64_t hid_to_physical(uint32_t hid);
uint64_t keysym_to_logical(xkb_keysym_t sym, uint32_t utf32);

}  // namespace oasis
