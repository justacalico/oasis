#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <drm_fourcc.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <unistd.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include "log.h"
#include "protocols.h"
#include "renderer.h"
#include "server.h"
#include "surface.h"
#include "linux-dmabuf-unstable-v1-server.h"

namespace oasis {

EGLImage Dmabuf::create_image(const EGLint* attrs) const {
  return create_image_(dpy_, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr,
                       attrs);
}

void Dmabuf::destroy_image(EGLImage image) const {
  if (destroy_image_ && image != EGL_NO_IMAGE) destroy_image_(dpy_, image);
}

void Dmabuf::image_to_texture(GLuint tex, EGLImage image) const {
  if (image_target_) image_target_(GL_TEXTURE_2D, image);
}

// -------------------------------------------------------------- DmabufBuffer

class DmabufBuffer : public Buffer {
 public:
  explicit DmabufBuffer(Dmabuf* dma) : dma(dma) {}
  ~DmabufBuffer() override {
    if (tex) glDeleteTextures(1, &tex);
    dma->destroy_image(image);
  }

  Dmabuf* dma;
  EGLImage image = EGL_NO_IMAGE;
  GLuint tex = 0;
  bool is_opaque = false;

  bool opaque() const override { return is_opaque; }

  GLuint texture(Renderer*) override {
    if (tex == 0) {
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      dma->image_to_texture(tex, image);
    }
    return tex;
  }
};

// -------------------------------------------------------------- DmabufParams

class DmabufParams : public ResourceBound {
 public:
  explicit DmabufParams(Dmabuf* dma) : dma(dma) {}
  ~DmabufParams() {
    for (auto& pl : planes)
      if (pl.fd >= 0) close(pl.fd);
  }

  Dmabuf* dma;
  int32_t w = 0, h = 0;
  uint32_t format = 0, flags = 0;
  struct {
    int fd = -1;
    uint32_t idx = 0, offset = 0, stride = 0;
    uint64_t modifier = 0;
    bool used = false;
  } planes[4];
  int nplanes = 0;

  void add(wl_resource* res, int32_t fd, uint32_t plane_idx, uint32_t offset,
           uint32_t stride, uint32_t mod_hi, uint32_t mod_lo);
  void create(wl_client* client, wl_resource* res, int32_t w, int32_t h,
              uint32_t format, uint32_t flags);
  void create_immed(wl_client* client, wl_resource* res, uint32_t buffer_id,
                    int32_t w, int32_t h, uint32_t format, uint32_t flags);

 private:
  bool import(wl_resource* buffer_res);
};

void DmabufParams::add(wl_resource* res, int32_t fd, uint32_t plane_idx,
                       uint32_t offset, uint32_t stride, uint32_t mod_hi,
                       uint32_t mod_lo) {
  if (plane_idx >= 4) {
    wl_resource_post_error(res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
                           "plane index out of range");
    close(fd);
    return;
  }
  if (planes[plane_idx].used) {
    wl_resource_post_error(res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
                           "plane already set");
    close(fd);
    return;
  }
  planes[plane_idx] = {fd, plane_idx, offset, stride,
                       ((uint64_t)mod_hi << 32) | mod_lo, true};
  if ((int)plane_idx + 1 > nplanes) nplanes = plane_idx + 1;
}

bool DmabufParams::import(wl_resource* buffer_res) {
  static const EGLint plane_attr[][3] = {
      {EGL_DMA_BUF_PLANE0_FD_EXT, EGL_DMA_BUF_PLANE0_OFFSET_EXT,
       EGL_DMA_BUF_PLANE0_PITCH_EXT},
      {EGL_DMA_BUF_PLANE1_FD_EXT, EGL_DMA_BUF_PLANE1_OFFSET_EXT,
       EGL_DMA_BUF_PLANE1_PITCH_EXT},
      {EGL_DMA_BUF_PLANE2_FD_EXT, EGL_DMA_BUF_PLANE2_OFFSET_EXT,
       EGL_DMA_BUF_PLANE2_PITCH_EXT},
      {EGL_DMA_BUF_PLANE3_FD_EXT, EGL_DMA_BUF_PLANE3_OFFSET_EXT,
       EGL_DMA_BUF_PLANE3_PITCH_EXT},
  };
  static const EGLint mod_attr[][2] = {
      {EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT},
      {EGL_DMA_BUF_PLANE1_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE1_MODIFIER_HI_EXT},
      {EGL_DMA_BUF_PLANE2_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE2_MODIFIER_HI_EXT},
      {EGL_DMA_BUF_PLANE3_MODIFIER_LO_EXT, EGL_DMA_BUF_PLANE3_MODIFIER_HI_EXT},
  };

  std::vector<EGLint> attrs;
  attrs.insert(attrs.end(),
               {EGL_WIDTH, w, EGL_HEIGHT, h, EGL_LINUX_DRM_FOURCC_EXT,
                (EGLint)format});
  int fds[4] = {-1, -1, -1, -1};
  for (int i = 0; i < nplanes; i++) {
    if (!planes[i].used) {
      wl_resource_post_error(res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                             "missing plane");
      return false;
    }
    attrs.insert(attrs.end(),
                 {plane_attr[i][0], planes[i].fd, plane_attr[i][1],
                  (EGLint)planes[i].offset, plane_attr[i][2],
                  (EGLint)planes[i].stride});
    if (planes[i].modifier != DRM_FORMAT_MOD_INVALID &&
        planes[i].modifier != DRM_FORMAT_MOD_LINEAR) {
      attrs.insert(attrs.end(),
                   {mod_attr[i][0],
                    (EGLint)(planes[i].modifier & 0xffffffff), mod_attr[i][1],
                    (EGLint)(planes[i].modifier >> 32)});
    }
    fds[i] = planes[i].fd;
    planes[i].fd = -1;
  }
  if (flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT)
    attrs.insert(attrs.end(), {EGL_YUV_COLOR_SPACE_HINT_EXT, 0});
  attrs.push_back(EGL_NONE);

  EGLImage image = dma->create_image(attrs.data());
  for (int fd : fds)
    if (fd >= 0) close(fd);
  if (image == EGL_NO_IMAGE) {
    LOGE("dmabuf import failed: egl error 0x%x", eglGetError());
    return false;
  }
  auto* buf = new DmabufBuffer(dma);
  buf->image = image;
  buf->width = w;
  buf->height = h;
  buf->is_opaque =
      (format == DRM_FORMAT_XRGB8888 || format == DRM_FORMAT_XBGR8888);
  attach_resource(buffer_res, buf, &buffer_impl, &Buffer::resource_destroyed);
  return true;
}

void DmabufParams::create(wl_client* client, wl_resource* res, int32_t w_,
                          int32_t h_, uint32_t format_, uint32_t flags_) {
  w = w_;
  h = h_;
  format = format_;
  flags = flags_;
  if (nplanes == 0 || w <= 0 || h <= 0) {
    zwp_linux_buffer_params_v1_send_failed(res);
    return;
  }
  // id 0: the wire id is allocated by the marshalling code for new_id args
  wl_resource* bres = wl_resource_create(client, &wl_buffer_interface, 1, 0);
  if (!import(bres)) {
    wl_resource_destroy(bres);
    zwp_linux_buffer_params_v1_send_failed(res);
    return;
  }
  zwp_linux_buffer_params_v1_send_created(res, bres);
}

void DmabufParams::create_immed(wl_client* client, wl_resource* res,
                                uint32_t buffer_id, int32_t w_, int32_t h_,
                                uint32_t format_, uint32_t flags_) {
  w = w_;
  h = h_;
  format = format_;
  flags = flags_;
  if (nplanes == 0 || w <= 0 || h <= 0) {
    wl_resource_post_error(
        res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
        "invalid buffer params");
    return;
  }
  wl_resource* bres =
      wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);
  if (!import(bres)) {
    wl_resource_destroy(bres);
    wl_resource_post_error(
        res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
        "import failed");
  }
}

static const struct zwp_linux_buffer_params_v1_interface params_impl = {
    destroy_request,
    REQ(DmabufParams, add),
    REQ(DmabufParams, create),
    REQ(DmabufParams, create_immed),
};

// ------------------------------------------------------- zwp_linux_dmabuf_v1

static const struct zwp_linux_dmabuf_v1_interface dmabuf_impl = {
    destroy_request, REQ(Dmabuf, create_params),
};

Dmabuf::Dmabuf(Server* server) {
  dpy_ = server->backend->egl_display();
  create_image_ =
      (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  destroy_image_ =
      (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  query_formats_ = (PFNEGLQUERYDMABUFFORMATSEXTPROC)eglGetProcAddress(
      "eglQueryDmaBufFormatsEXT");
  query_modifiers_ = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)eglGetProcAddress(
      "eglQueryDmaBufModifiersEXT");
  image_target_ = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
      "glEGLImageTargetTexture2DOES");
  if (!create_image_ || !image_target_) {
    LOGE("no EGL image support; dmabuf disabled");
    return;
  }
  global_.create(server->display, &zwp_linux_dmabuf_v1_interface, 3,
                 [this](wl_client* c, uint32_t v, uint32_t id) {
                   bind(c, v, id);
                 });
}

void Dmabuf::create_params(wl_client* client, wl_resource* res, uint32_t id) {
  bind_resource(client, &zwp_linux_buffer_params_v1_interface,
                wl_resource_get_version(res), id, new DmabufParams(this),
                &params_impl);
}

void Dmabuf::bind(wl_client* client, uint32_t version, uint32_t id) {
  version = std::min<uint32_t>(version, 3);
  wl_resource* res = wl_resource_create(
      client, &zwp_linux_dmabuf_v1_interface, version, id);
  wl_resource_set_implementation(res, &dmabuf_impl, this, nullptr);

  // advertise what our EGL implementation can import
  std::vector<uint32_t> formats;
  std::vector<uint64_t> mods;
  if (query_formats_ && query_modifiers_) {
    EGLint n = 0;
    if (query_formats_(dpy_, 0, nullptr, &n) && n > 0) {
      std::vector<EGLint> f(n);
      query_formats_(dpy_, n, f.data(), &n);
      for (EGLint fmt : f) {
        formats.push_back((uint32_t)fmt);
        EGLint nm = 0;
        if (query_modifiers_(dpy_, fmt, 0, nullptr, nullptr, &nm) && nm > 0) {
          std::vector<EGLuint64KHR> m(nm);
          query_modifiers_(dpy_, fmt, nm, m.data(), nullptr, &nm);
          for (auto mod : m) mods.push_back(mod);
        }
      }
    }
  }
  if (formats.empty()) {
    formats = {DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888, DRM_FORMAT_ABGR8888,
               DRM_FORMAT_XBGR8888, DRM_FORMAT_RGB565};
  }
  for (uint32_t fmt : formats) {
    zwp_linux_dmabuf_v1_send_format(res, fmt);
    if (version >= ZWP_LINUX_DMABUF_V1_MODIFIER_SINCE_VERSION) {
      if (mods.empty()) {
        zwp_linux_dmabuf_v1_send_modifier(res, fmt, 0xffffffff, 0xffffffff);
      } else {
        for (uint64_t m : mods)
          zwp_linux_dmabuf_v1_send_modifier(res, fmt,
                                            (uint32_t)(m >> 32),
                                            (uint32_t)(m & 0xffffffff));
      }
    }
  }
}

}  // namespace oasis
