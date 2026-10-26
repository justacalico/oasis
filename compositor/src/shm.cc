#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include "log.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "protocols.h"
#include "renderer.h"
#include "server.h"
#include "surface.h"

namespace oasis {

void Buffer::resource_destroyed(wl_resource* res) {
  auto* buf = user_data<Buffer>(res);
  if (buf->taken) {
    buf->res = nullptr;
  } else {
    delete buf;
  }
}

// ------------------------------------------------------------------ ShmPool

// wl_shm_pool: a client-owned file mapping out of which wl_buffers are
// carved. We keep every mapping we ever made so buffers stay valid across
// pool resizes.
class ShmPool : public ResourceBound {
 public:
  ShmPool(int fd_, int32_t size_, void* data_)
      : fd(fd_), size(size_), data(data_) {
    maps.emplace_back(data_, (size_t)size_);
  }
  ~ShmPool() {
    for (auto& m : maps) munmap(m.first, m.second);
  }

  Fd fd;
  int32_t size;
  std::vector<std::pair<void*, size_t>> maps;  // all live mappings
  void* data;                                  // current mapping

  void create_buffer(wl_client* client, wl_resource* res, uint32_t id,
                     int32_t offset, int32_t width, int32_t height,
                     int32_t stride, uint32_t format);
  void resize(wl_resource* res, int32_t new_size);
};

class ShmBuffer : public Buffer {
 public:
  ShmPool* pool = nullptr;
  int32_t offset = 0;
  int32_t stride = 0;
  uint32_t format = 0;
  GLuint tex = 0;
  bool uploaded = false;

  ~ShmBuffer() override {
    if (tex) glDeleteTextures(1, &tex);
  }

  bool opaque() const override { return format == WL_SHM_FORMAT_XRGB8888; }
  GLuint texture(Renderer* r) override;
};

void ShmPool::create_buffer(wl_client* client, wl_resource* res, uint32_t id,
                            int32_t offset, int32_t width, int32_t height,
                            int32_t stride, uint32_t format) {
  if (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_FORMAT,
                           "unsupported format %u", format);
    return;
  }
  if (offset < 0 || width <= 0 || height <= 0 || stride < width * 4 ||
      (int64_t)offset + (int64_t)stride * height > size) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_STRIDE,
                           "buffer out of bounds");
    return;
  }
  auto* buf = new ShmBuffer();
  buf->width = width;
  buf->height = height;
  buf->pool = this;
  buf->offset = offset;
  buf->stride = stride;
  buf->format = format;
  bind_resource(client, &wl_buffer_interface, 1, id, buf, &buffer_impl,
                &Buffer::resource_destroyed);
}

void ShmPool::resize(wl_resource* res, int32_t new_size) {
  if (new_size <= size) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_FD,
                           "cannot shrink pool");
    return;
  }
  // The client has already grown the underlying file (often a sealed
  // memfd, which is why we must not ftruncate it ourselves).
  void* p = mmap(nullptr, new_size, PROT_READ, MAP_SHARED, fd.get(), 0);
  if (p == MAP_FAILED) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_FD, "mmap: %m");
    return;
  }
  maps.emplace_back(p, (size_t)new_size);
  data = p;
  size = new_size;
}

static const struct wl_shm_pool_interface pool_impl = {
    REQ(ShmPool, create_buffer), destroy_request, REQ(ShmPool, resize),
};

// -------------------------------------------------------------------- wl_shm

static const struct wl_shm_interface shm_impl = {REQ(Shm, create_pool)};

Shm::Shm(Server* server) : server_(server) {
  global_.create(server->display, &wl_shm_interface, 1,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   wl_resource* res =
                       wl_resource_create(c, &wl_shm_interface, v, id);
                   wl_resource_set_implementation(res, &shm_impl, this,
                                                  nullptr);
                   wl_shm_send_format(res, WL_SHM_FORMAT_ARGB8888);
                   wl_shm_send_format(res, WL_SHM_FORMAT_XRGB8888);
                 });
}

void Shm::create_pool(wl_client* client, wl_resource* res, uint32_t id,
                      int32_t fd, int32_t size) {
  if (size <= 0) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_FD, "bad size");
    close(fd);
    return;
  }
  void* data = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_FD, "mmap: %m");
    close(fd);
    return;
  }
  bind_resource(client, &wl_shm_pool_interface, wl_resource_get_version(res),
                id, new ShmPool(fd, size, data), &pool_impl);
}

GLuint ShmBuffer::texture(Renderer* r) {
  if (uploaded) return tex;
  if (tex == 0) {
    glGenTextures(1, &tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  glBindTexture(GL_TEXTURE_2D, tex);
  const uint8_t* src =
      static_cast<const uint8_t*>(pool->data) + offset;
  std::vector<uint8_t> repack;
  if (stride != width * 4) {
    repack.resize((size_t)width * 4 * height);
    for (int32_t row = 0; row < height; row++)
      memcpy(repack.data() + (size_t)row * width * 4,
             src + (size_t)row * stride, width * 4);
    src = repack.data();
  }
  // little-endian ARGB8888 memory order is B,G,R,A
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT,
               GL_UNSIGNED_BYTE, src);
  uploaded = true;
  return tex;
}

}  // namespace oasis
