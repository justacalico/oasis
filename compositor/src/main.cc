#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include "flutter.h"
#include "log.h"
#include "server.h"
#include "util.h"

using namespace oasis;

static std::string exe_dir() {
  std::error_code ec;
  auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec) return ".";
  return p.parent_path().string();
}

int main(int argc, char** argv) {
  Config cfg;
  std::string exe = exe_dir();
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--drm") {
      cfg.force_drm = true;
    } else if (a == "--nested") {
      cfg.force_drm = false;
      const char* host = getenv("WAYLAND_DISPLAY");
      setenv("OASIS_HOST_DISPLAY", host ? host : "", 1);
    } else if (a == "--bundle" && i + 1 < argc) {
      cfg.bundle_dir = argv[++i];
    } else if (a == "--engine-dir" && i + 1 < argc) {
      cfg.engine_dir = argv[++i];
    } else if (a == "--socket" && i + 1 < argc) {
      cfg.socket = argv[++i];
    } else if (a == "--help" || a == "-h") {
      printf("usage: oasis-comp [--drm|--nested] [--bundle DIR] "
             "[--engine-dir DIR] [--socket NAME]\n");
      return 0;
    } else {
      fprintf(stderr, "unknown argument: %s\n", a.c_str());
      return 1;
    }
  }

  if (cfg.engine_dir.empty()) {
    if (const char* e = getenv("OASIS_ENGINE_DIR"))
      cfg.engine_dir = e;
    else if (dir_exists(exe + "/engine"))
      cfg.engine_dir = exe + "/engine";
    else if (dir_exists(exe + "/../engine"))
      cfg.engine_dir = exe + "/../engine";
    else
      cfg.engine_dir = exe + "/../compositor/engine";
  }
  if (cfg.bundle_dir.empty()) {
    if (const char* b = getenv("OASIS_BUNDLE"))
      cfg.bundle_dir = b;
    else if (dir_exists(exe + "/bundle"))
      cfg.bundle_dir = exe + "/bundle";
    else {
      for (const char* c : {"../build/linux/x64/release/bundle",
                            "../../build/linux/x64/release/bundle"}) {
        std::string p = exe + "/" + c;
        if (dir_exists(p + "/data")) {
          cfg.bundle_dir = p;
          break;
        }
      }
    }
  }

  Server server;
  if (!server.init(cfg)) {
    LOGE("compositor init failed");
    return 1;
  }
  server.run();
  return 0;
}
