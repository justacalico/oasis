# oasis-comp

The Oasis Wayland compositor. It embeds the Flutter engine for the shell UI
(desktop, dock, settings) and serves real Wayland clients over xdg-shell,
wl_shm and linux-dmabuf.

## Architecture

- `src/wl.h` — C++ wrappers over libwayland: `REQ`/`LREQ` member-function
  dispatch for generated tables, RAII `Global`, `EventSource`, `Listener`,
  `Fd`, and `bind_resource` for resource-owned objects.
- `src/backend_wayland.cc` — nested backend: runs as an xdg toplevel on a
  host compositor. This is the dev path.
- `src/backend_drm.cc` — KMS/GBM/libinput backend via libseat for real
  hardware (logind or seatd session required).
- `src/surface.cc`, `src/shm.cc`, `src/xdg.cc`, `src/dmabuf.cc`,
  `src/data_device.cc`, `src/decoration.cc`, `src/viewporter.cc` —
  server-side Wayland protocol implementations, one class per object.
- `src/protocols.h` — the global-owning protocol classes (`Shm`, `Dmabuf`,
  `DataDeviceManager`, `DecorationManager`, `Viewporter`, `OutputGlobal`).
- `src/renderer.cc` — GLES2 scene renderer.
- `src/flutter.cc` — Flutter embedder (engine FBO composited as the shell
  layer, input injection, `oasis.wm` JSON channel for window management).
- `src/seat.cc` — pointer/keyboard routing between clients and the shell.

## Running

```
./scripts/fetch_flutter_engine.sh   # downloads matching embedder engine
flutter build linux --debug         # kernel_blob bundle for the JIT engine
./scripts/run_compositor.sh         # nested inside current session
./scripts/run_compositor.sh --drm   # on hardware from a VT
```

The compositor exports `WAYLAND_DISPLAY` + `OASIS_COMPOSITOR=1`, so apps
launched from the shell grid (or any client pointed at the socket) run on it.

## Protocol coverage

wl_compositor, wl_subcompositor, wl_shm, wl_seat (pointer+keyboard),
wl_output, wl_data_device_manager (clipboard + dnd), xdg_wm_base (toplevels,
popups, positioner), zxdg_decoration_manager_v1, wp_viewporter,
zwp_linux_dmabuf_v1 (v3, EGL import). No XWayland.
