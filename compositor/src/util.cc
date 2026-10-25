#include "util.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>

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

bool file_exists(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

bool dir_exists(const std::string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::vector<uint8_t> read_file(const std::string& path) {
  int fd = open(path.c_str(), O_RDONLY);
  if (fd < 0) return {};
  std::vector<uint8_t> data;
  struct stat st;
  if (fstat(fd, &st) == 0 && st.st_size > 0) {
    data.resize(st.st_size);
    size_t off = 0;
    while (off < data.size()) {
      ssize_t n = read(fd, data.data() + off, data.size() - off);
      if (n <= 0) {
        data.clear();
        break;
      }
      off += n;
    }
  }
  close(fd);
  return data;
}

void Region::subtract(const Rect& hole) {
  std::vector<Rect> out;
  for (const Rect& q : rects) {
    if (q.x >= hole.x + hole.w || q.x + q.w <= hole.x ||
        q.y >= hole.y + hole.h || q.y + q.h <= hole.y) {
      out.push_back(q);
      continue;
    }
    if (q.y < hole.y) out.push_back({q.x, q.y, q.w, hole.y - q.y});
    if (q.y + q.h > hole.y + hole.h)
      out.push_back({q.x, hole.y + hole.h, q.w,
                     q.y + q.h - (hole.y + hole.h)});
    if (q.x < hole.x)
      out.push_back(
          {q.x, std::max(q.y, hole.y), hole.x - q.x,
           std::min(q.y + q.h, hole.y + hole.h) - std::max(q.y, hole.y)});
    if (q.x + q.w > hole.x + hole.w)
      out.push_back({hole.x + hole.w, std::max(q.y, hole.y),
                     q.x + q.w - (hole.x + hole.w),
                     std::min(q.y + q.h, hole.y + hole.h) -
                         std::max(q.y, hole.y)});
  }
  rects = std::move(out);
}

}  // namespace oasis
