#include <unistd.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "log.h"
#include "flutter.h"
#include "server.h"

using namespace oasis;

static bool dir_exists(const std::string& p) {
  struct stat st;
  return stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

static std::string exe_dir() {
  char buf[4096];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return ".";
  buf[n] = 0;
  std::string p(buf);
  return p.substr(0, p.rfind('/'));
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
      setenv("OASIS_HOST_DISPLAY", getenv("WAYLAND_DISPLAY") ?: "", 1);
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
  if (server.flutter) server.flutter->shutdown();
  return 0;
}
