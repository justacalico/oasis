cmake_minimum_required(VERSION 3.20)
project(oasis-comp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(WL REQUIRED IMPORTED_TARGET
  wayland-server wayland-client wayland-cursor wayland-egl
  egl glesv2 gbm libdrm libinput libseat libudev xkbcommon)

pkg_get_variable(WL_PROTO_DIR wayland-protocols pkgdatadir)
find_program(WAYLAND_SCANNER NAMES wayland-scanner REQUIRED)

set(PROTO_GEN ${CMAKE_BINARY_DIR}/protocol)
file(MAKE_DIRECTORY ${PROTO_GEN})

set(PROTOCOLS
  ${WL_PROTO_DIR}/stable/xdg-shell/xdg-shell.xml
  ${WL_PROTO_DIR}/unstable/linux-dmabuf/linux-dmabuf-unstable-v1.xml
  ${WL_PROTO_DIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
  ${WL_PROTO_DIR}/stable/viewporter/viewporter.xml
)

set(PROTO_SRCS "")
foreach(xml ${PROTOCOLS})
  get_filename_component(name ${xml} NAME_WE)
  set(hdr ${PROTO_GEN}/${name}-server.h)
  set(src ${PROTO_GEN}/${name}-protocol.c)
  add_custom_command(OUTPUT ${hdr}
    COMMAND ${WAYLAND_SCANNER} server-header ${xml} ${hdr}
    DEPENDS ${xml})
  add_custom_command(OUTPUT ${src}
    COMMAND ${WAYLAND_SCANNER} private-code ${xml} ${src}
    DEPENDS ${xml})
  list(APPEND PROTO_SRCS ${hdr} ${src})
endforeach()

# the nested Wayland backend is also a client of the host compositor
set(CLIENT_HDR ${PROTO_GEN}/xdg-shell-client.h)
add_custom_command(OUTPUT ${CLIENT_HDR}
  COMMAND ${WAYLAND_SCANNER} client-header
    ${WL_PROTO_DIR}/stable/xdg-shell/xdg-shell.xml ${CLIENT_HDR}
  DEPENDS ${WL_PROTO_DIR}/stable/xdg-shell/xdg-shell.xml)
list(APPEND PROTO_SRCS ${CLIENT_HDR})

add_executable(oasis-comp
  ${PROTO_SRCS}
  src/main.cc
  src/server.cc
  src/util.cc
  src/json.cc
  src/renderer.cc
  src/surface.cc
  src/shm.cc
  src/seat.cc
  src/xdg.cc
  src/data_device.cc
  src/dmabuf.cc
  src/decoration.cc
  src/viewporter.cc
  src/backend_wayland.cc
  src/backend_drm.cc
  src/flutter.cc
  src/keymap.cc
)

target_include_directories(oasis-comp PRIVATE
  src ${CMAKE_BINARY_DIR}/protocol ${CMAKE_CURRENT_SOURCE_DIR}/third_party)
target_link_libraries(oasis-comp PRIVATE PkgConfig::WL dl)
