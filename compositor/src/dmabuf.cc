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
#include "renderer.h"
#include "server.h"
#include "surface.h"
#include "linux-dmabuf-unstable-v1-server.h"

namespace oasis {

static PFNEGLCREATEIMAGEKHRPROC egl_create_image = nullptr;
static PFNEGLDESTROYIMAGEKHRPROC egl_destroy_image = nullptr;
static PFNEGLQUERYDMABUFFORMATSEXTPROC egl_query_formats = nullptr;
static PFNEGLQUERYDMABUFMODIFIERSEXTPROC egl_query_modifiers = nullptr;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC gl_egl_image_target = nullptr;
static EGLDisplay g_dpy = EGL_NO_DISPLAY;

class DmabufBuffer : public Buffer {
 public:
  EGLImage image = EGL_NO_IMAGE;
  GLuint tex = 0;
  bool is_opaque = false;

  ~DmabufBuffer() override {
    if (tex) glDeleteTextures(1, &tex);
    if (image != EGL_NO_IMAGE && egl_destroy_image && g_dpy)
      egl_destroy_image(g_dpy, image);
  }

  bool opaque() const override { return is_opaque; }

  GLuint texture(Renderer*) override {
    if (tex == 0 && gl_egl_image_target) {
      glGenTextures(1, &tex);
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
      gl_egl_image_target(GL_TEXTURE_2D, image);
    }
    return tex;
  }
};

struct DmabufParams {
  Server* server;
  wl_resource* res;
  int32_t w = 0, h = 0;
  uint32_t format = 0, flags = 0;
  struct {
    int fd = -1;
    uint32_t idx = 0, offset = 0, stride = 0;
    uint64_t modifier = 0;
    bool used = false;
  } planes[4];
  int nplanes = 0;
};

extern void buffer_resource_destroy(wl_resource* res);
extern const struct wl_buffer_interface* shm_buffer_impl();

static void params_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void params_add(wl_client*, wl_resource* res, int32_t fd,
                       uint32_t plane_idx, uint32_t offset, uint32_t stride,
                       uint32_t mod_hi, uint32_t mod_lo) {
  auto* p = static_cast<DmabufParams*>(wl_resource_get_user_data(res));
  if (plane_idx >= 4) {
    wl_resource_post_error(
        res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_IDX,
        "plane index out of range");
    close(fd);
    return;
  }
  if (p->planes[plane_idx].used) {
    wl_resource_post_error(
        res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_PLANE_SET,
        "plane already set");
    close(fd);
    return;
  }
  p->planes[plane_idx] = {fd, plane_idx, offset, stride,
                          ((uint64_t)mod_hi << 32) | mod_lo, true};
  if ((int)plane_idx + 1 > p->nplanes) p->nplanes = plane_idx + 1;
}

static bool import_dmabuf(DmabufParams* p, wl_resource* buffer_res) {
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
               {EGL_WIDTH, p->w, EGL_HEIGHT, p->h, EGL_LINUX_DRM_FOURCC_EXT,
                (EGLint)p->format});
  int fds[4] = {-1, -1, -1, -1};
  for (int i = 0; i < p->nplanes; i++) {
    if (!p->planes[i].used) {
      wl_resource_post_error(p->res,
                             ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INCOMPLETE,
                             "missing plane");
      return false;
    }
    attrs.insert(attrs.end(),
                 {plane_attr[i][0], p->planes[i].fd, plane_attr[i][1],
                  (EGLint)p->planes[i].offset, plane_attr[i][2],
                  (EGLint)p->planes[i].stride});
    if (p->planes[i].modifier != DRM_FORMAT_MOD_INVALID &&
        p->planes[i].modifier != DRM_FORMAT_MOD_LINEAR) {
      attrs.insert(attrs.end(),
                   {mod_attr[i][0],
                    (EGLint)(p->planes[i].modifier & 0xffffffff),
                    mod_attr[i][1], (EGLint)(p->planes[i].modifier >> 32)});
    }
    fds[i] = p->planes[i].fd;
    p->planes[i].fd = -1;
  }
  if (p->flags & ZWP_LINUX_BUFFER_PARAMS_V1_FLAGS_Y_INVERT)
    attrs.insert(attrs.end(), {EGL_YUV_COLOR_SPACE_HINT_EXT, 0});
  attrs.push_back(EGL_NONE);

  EGLImage image = egl_create_image(
      g_dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, nullptr, attrs.data());
  for (int i = 0; i < 4; i++)
    if (fds[i] >= 0) close(fds[i]);
  if (image == EGL_NO_IMAGE) {
    LOGE("dmabuf import failed: egl error 0x%x", eglGetError());
    return false;
  }
  auto* buf = new DmabufBuffer();
  buf->res = buffer_res;
  buf->image = image;
  buf->width = p->w;
  buf->height = p->h;
  buf->is_opaque =
      (p->format == DRM_FORMAT_XRGB8888 || p->format == DRM_FORMAT_XBGR8888);
  wl_resource_set_implementation(buffer_res, shm_buffer_impl(), buf,
                               buffer_resource_destroy);
  return true;
}

static void params_create(wl_client* client, wl_resource* res, int32_t w,
                          int32_t h, uint32_t format, uint32_t flags) {
  auto* p = static_cast<DmabufParams*>(wl_resource_get_user_data(res));
  p->w = w;
  p->h = h;
  p->format = format;
  p->flags = flags;
  if (p->nplanes == 0 || w <= 0 || h <= 0) {
    zwp_linux_buffer_params_v1_send_failed(res);
    return;
  }
  // id 0: the wire id is allocated by the marshalling code for new_id args
  wl_resource* bres =
      wl_resource_create(client, &wl_buffer_interface, 1, 0);
  if (!import_dmabuf(p, bres)) {
    wl_resource_destroy(bres);
    zwp_linux_buffer_params_v1_send_failed(res);
    return;
  }
  zwp_linux_buffer_params_v1_send_created(res, bres);
}

static void params_create_immed(wl_client* client, wl_resource* res,
                                uint32_t buffer_id, int32_t w, int32_t h,
                                uint32_t format, uint32_t flags) {
  auto* p = static_cast<DmabufParams*>(wl_resource_get_user_data(res));
  p->w = w;
  p->h = h;
  p->format = format;
  p->flags = flags;
  if (p->nplanes == 0 || w <= 0 || h <= 0) {
    wl_resource_post_error(
        res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
        "invalid buffer params");
    return;
  }
  wl_resource* bres =
      wl_resource_create(client, &wl_buffer_interface, 1, buffer_id);
  if (!import_dmabuf(p, bres)) {
    wl_resource_destroy(bres);
    wl_resource_post_error(
        res, ZWP_LINUX_BUFFER_PARAMS_V1_ERROR_INVALID_WL_BUFFER,
        "import failed");
  }
}

static const struct zwp_linux_buffer_params_v1_interface params_impl = {
    params_destroy, params_add, params_create, params_create_immed,
};

static void params_resource_destroy(wl_resource* res) {
  auto* p = static_cast<DmabufParams*>(wl_resource_get_user_data(res));
  for (auto& pl : p->planes)
    if (pl.fd >= 0) close(pl.fd);
  delete p;
}

static void dmabuf_destroy(wl_client*, wl_resource* res) {
  wl_resource_destroy(res);
}

static void dmabuf_create_params(wl_client* client, wl_resource* res,
                                 uint32_t id) {
  auto* server = static_cast<Server*>(wl_resource_get_user_data(res));
  wl_resource* pres =
      wl_resource_create(client, &zwp_linux_buffer_params_v1_interface,
                         wl_resource_get_version(res), id);
  auto* p = new DmabufParams();
  p->server = server;
  p->res = pres;
  wl_resource_set_implementation(pres, &params_impl, p,
                                 params_resource_destroy);
}

static const struct zwp_linux_dmabuf_v1_interface dmabuf_impl = {
    dmabuf_destroy, dmabuf_create_params,
};

static void dmabuf_bind(wl_client* client, void* data, uint32_t version,
                        uint32_t id) {
  auto* server = static_cast<Server*>(data);
  version = std::min<uint32_t>(version, 3);
  wl_resource* res = wl_resource_create(
      client, &zwp_linux_dmabuf_v1_interface, version, id);
  wl_resource_set_implementation(res, &dmabuf_impl, server, nullptr);

  // advertise what our EGL implementation can import
  std::vector<uint32_t> formats;
  std::vector<uint64_t> mods;
  if (egl_query_formats && egl_query_modifiers) {
    EGLint n = 0;
    if (egl_query_formats(g_dpy, 0, nullptr, &n) && n > 0) {
      std::vector<EGLint> f(n);
      egl_query_formats(g_dpy, n, f.data(), &n);
      for (EGLint fmt : f) {
        formats.push_back((uint32_t)fmt);
        EGLint nm = 0;
        if (egl_query_modifiers(g_dpy, fmt, 0, nullptr, nullptr, &nm) &&
            nm > 0) {
          std::vector<EGLuint64KHR> m(nm);
          egl_query_modifiers(g_dpy, fmt, nm, m.data(), nullptr, &nm);
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

void dmabuf_global_init(Server* server) {
  EGLDisplay dpy = server->backend->egl_display();
  g_dpy = dpy;
  egl_create_image =
      (PFNEGLCREATEIMAGEKHRPROC)eglGetProcAddress("eglCreateImageKHR");
  egl_destroy_image =
      (PFNEGLDESTROYIMAGEKHRPROC)eglGetProcAddress("eglDestroyImageKHR");
  egl_query_formats = (PFNEGLQUERYDMABUFFORMATSEXTPROC)eglGetProcAddress(
      "eglQueryDmaBufFormatsEXT");
  egl_query_modifiers = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)eglGetProcAddress(
      "eglQueryDmaBufModifiersEXT");
  gl_egl_image_target = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)eglGetProcAddress(
      "glEGLImageTargetTexture2DOES");
  if (!egl_create_image || !gl_egl_image_target) {
    LOGE("no EGL image support; dmabuf disabled");
    return;
  }
  wl_global_create(server->display, &zwp_linux_dmabuf_v1_interface, 3, server,
                   dmabuf_bind);
}

}  // namespace oasis
