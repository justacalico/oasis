#include "util.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

namespace oasis {

uint32_t now_msec() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

uint64_t now_usec() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000ULL + ts.tv_nsec / 1000;
}

int memfd_create_cloexec(const char* name) {
  return syscall(SYS_memfd_create, name, MFD_CLOEXEC);
}

bool write_all(int fd, const void* data, size_t size) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  while (size > 0) {
    ssize_t n = write(fd, p, size);
    if (n <= 0) return false;
    p += n;
    size -= n;
  }
  return true;
}

std::string read_fd_to_string(int fd) {
  std::string out;
  char buf[8192];
  ssize_t n;
  while ((n = read(fd, buf, sizeof(buf))) > 0) out.append(buf, n);
  return out;
}

}  // namespace oasis
