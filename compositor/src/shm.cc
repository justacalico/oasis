#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include "log.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include "renderer.h"
#include "server.h"
#include "surface.h"

namespace oasis {

// ------------------------------------------------------------------ ShmPool

struct ShmPool {
  wl_resource* res;
  int fd;
  int32_t size;
  std::vector<std::pair<void*, size_t>> maps;  // all live mappings
  void* data;                                  // current mapping
};

struct ShmBacking {
  ShmPool* pool;
  int32_t offset;
  int32_t stride;
  uint32_t format;
};

class ShmBuffer : public Buffer {
 public:
  ShmBacking backing{};
  GLuint tex = 0;
  bool uploaded = false;

  ~ShmBuffer() override {
    if (tex) glDeleteTextures(1, &tex);
  }

  bool opaque() const override {
    return backing.format == WL_SHM_FORMAT_XRGB8888;
  }
  GLuint texture(Renderer* r) override;
};

static void buffer_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static const struct wl_buffer_interface buffer_impl = {buffer_destroy};

// Shared wl_buffer resource destructor: frees the Buffer unless a surface
// took ownership of it.
void buffer_resource_destroy(wl_resource* res) {
  auto* buf = static_cast<Buffer*>(wl_resource_get_user_data(res));
  if (!buf) return;
  if (buf->taken) {
    buf->res = nullptr;
  } else {
    delete buf;
  }
}

const struct wl_buffer_interface* shm_buffer_impl() {
  return &buffer_impl;
}

Buffer* buffer_from_resource(wl_resource* res) {
  return static_cast<Buffer*>(wl_resource_get_user_data(res));
}

void buffer_take(Surface* s, wl_resource* res) {
  auto* buf = buffer_from_resource(res);
  if (!buf) {
    wl_resource_post_error(s->res, 0 /*invalid buffer*/,
                           "unknown buffer");
    return;
  }
  buf->taken = true;
  s->buffer.reset(buf);
}

static void pool_create_buffer(wl_client* client, wl_resource* res,
                               uint32_t id, int32_t offset, int32_t width,
                               int32_t height, int32_t stride,
                               uint32_t format) {
  auto* pool = static_cast<ShmPool*>(wl_resource_get_user_data(res));
  if (format != WL_SHM_FORMAT_ARGB8888 && format != WL_SHM_FORMAT_XRGB8888) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_FORMAT,
                           "unsupported format %u", format);
    return;
  }
  if (offset < 0 || width <= 0 || height <= 0 || stride < width * 4 ||
      (int64_t)offset + (int64_t)stride * height > pool->size) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_STRIDE,
                           "buffer out of bounds");
    return;
  }
  wl_resource* bres =
      wl_resource_create(client, &wl_buffer_interface, 1, id);
  auto* buf = new ShmBuffer();
  buf->res = bres;
  buf->width = width;
  buf->height = height;
  buf->backing = {pool, offset, stride, format};
  wl_resource_set_implementation(bres, &buffer_impl, buf,
                                 buffer_resource_destroy);
}

static void pool_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void pool_resize(wl_client*, wl_resource* res, int32_t size) {
  auto* pool = static_cast<ShmPool*>(wl_resource_get_user_data(res));
  if (size <= pool->size) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_FD,
                           "cannot shrink pool");
    return;
  }
  // The client has already grown the underlying file (often a sealed
  // memfd, which is why we must not ftruncate it ourselves).
  void* data = mmap(nullptr, size, PROT_READ, MAP_SHARED, pool->fd, 0);
  if (data == MAP_FAILED) {
    wl_resource_post_error(res, WL_SHM_ERROR_INVALID_FD, "mmap: %m");
    return;
  }
  pool->maps.emplace_back(data, (size_t)size);
  pool->data = data;
  pool->size = size;
}

static const struct wl_shm_pool_interface pool_impl = {
    pool_create_buffer, pool_destroy, pool_resize,
};

static void pool_resource_destroy(wl_resource* res) {
  auto* pool = static_cast<ShmPool*>(wl_resource_get_user_data(res));
  for (auto& m : pool->maps) munmap(m.first, m.second);
  close(pool->fd);
  delete pool;
}

static void shm_create_pool(wl_client* client, wl_resource* res, uint32_t id,
                            int fd, int32_t size) {
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
  wl_resource* pres = wl_resource_create(
      client, &wl_shm_pool_interface, wl_resource_get_version(res), id);
  auto* pool = new ShmPool{pres, fd, size, {{data, (size_t)size}}, data};
  wl_resource_set_implementation(pres, &pool_impl, pool,
                                 pool_resource_destroy);
}

static const struct wl_shm_interface shm_impl = {shm_create_pool};

static void shm_bind(wl_client* client, void*, uint32_t version, uint32_t id) {
  wl_resource* res =
      wl_resource_create(client, &wl_shm_interface, version, id);
  wl_resource_set_implementation(res, &shm_impl, nullptr, nullptr);
  wl_shm_send_format(res, WL_SHM_FORMAT_ARGB8888);
  wl_shm_send_format(res, WL_SHM_FORMAT_XRGB8888);
}

void shm_global_init(Server* server) {
  wl_global_create(server->display, &wl_shm_interface, 1, nullptr, shm_bind);
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
  const uint8_t* src = static_cast<const uint8_t*>(backing.pool->data) +
                       backing.offset;
  std::vector<uint8_t> repack;
  if (backing.stride != width * 4) {
    repack.resize((size_t)width * 4 * height);
    for (int32_t row = 0; row < height; row++)
      memcpy(repack.data() + (size_t)row * width * 4,
             src + (size_t)row * backing.stride, width * 4);
    src = repack.data();
  }
  // little-endian ARGB8888 memory order is B,G,R,A
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_BGRA_EXT,
               GL_UNSIGNED_BYTE, src);
  uploaded = true;
  return tex;
}

}  // namespace oasis
