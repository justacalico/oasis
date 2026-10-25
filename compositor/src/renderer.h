#pragma once
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <cstdint>
#include "util.h"

namespace oasis {

// GLES2 renderer that draws textured quads (client surfaces, the Flutter
// shell texture, the cursor) into each output's framebuffer.
class Renderer {
 public:
  bool init();
  void shutdown();

  void begin(int32_t w, int32_t h);
  void clear(float r, float g, float b, float a);
  // transform is a wl_output_transform value applied to texture coordinates;
  // force_opaque ignores the texture alpha channel (XRGB buffers).
  void texture(GLuint tex, int32_t x, int32_t y, int32_t w, int32_t h,
               int transform = 0, bool force_opaque = false,
               float alpha = 1.0f);
  void rect(int32_t x, int32_t y, int32_t w, int32_t h, float r, float g,
            float b, float a);
  GLuint texture_from_pixels(int32_t w, int32_t h, const void* rgba);
  GLuint texture_from_egl_image(EGLImage image, int32_t w, int32_t h);

 private:
  GLuint tex_prog_ = 0, solid_prog_ = 0;
  GLuint vbo_ = 0;
  int32_t out_w_ = 0, out_h_ = 0;
  GLint tex_pos_ = -1, tex_uv_ = -1, tex_tex_ = -1, tex_alpha_ = -1,
        tex_force_opaque_ = -1, tex_transform_ = -1, tex_out_size_ = -1;
  GLint solid_pos_ = -1, solid_color_ = -1, solid_out_size_ = -1;
  GLuint compile(GLenum type, const char* src);
  GLuint link(const char* vs, const char* fs);
};

}  // namespace oasis
