#include "renderer.h"

#include "log.h"
#include <cstdlib>
#include <cstring>

namespace oasis {

// transform values match wl_output_transform. v_uv runs over the quad; the
// shader permutes it for rotated/flipped buffers.
static const char* kTexVS = R"(#version 100
attribute vec2 pos;
attribute vec2 uv;
uniform vec2 out_size;
uniform int transform;
varying vec2 v_uv;
void main() {
  vec2 ndc = vec2(pos.x / out_size.x * 2.0 - 1.0,
                  1.0 - pos.y / out_size.y * 2.0);
  gl_Position = vec4(ndc, 0.0, 1.0);
  vec2 t = uv;
  if (transform == 1) t = vec2(uv.y, 1.0 - uv.x);
  else if (transform == 2) t = vec2(1.0 - uv.x, 1.0 - uv.y);
  else if (transform == 3) t = vec2(1.0 - uv.y, uv.x);
  else if (transform == 4) t = vec2(1.0 - uv.x, uv.y);
  else if (transform == 5) t = vec2(uv.y, uv.x);
  else if (transform == 6) t = vec2(uv.x, 1.0 - uv.y);
  else if (transform == 7) t = vec2(1.0 - uv.y, 1.0 - uv.x);
  v_uv = t;
})";

static const char* kTexFS = R"(#version 100
precision mediump float;
varying vec2 v_uv;
uniform sampler2D tex;
uniform float alpha;
uniform float force_opaque;
void main() {
  vec4 c = texture2D(tex, v_uv);
  c.a = mix(c.a, 1.0, force_opaque);
  gl_FragColor = vec4(c.rgb * c.a, c.a) * alpha;
})";

static const char* kSolidVS = R"(#version 100
attribute vec2 pos;
uniform vec2 out_size;
void main() {
  gl_Position = vec4(pos.x / out_size.x * 2.0 - 1.0,
                     1.0 - pos.y / out_size.y * 2.0, 0.0, 1.0);
})";

static const char* kSolidFS = R"(#version 100
precision mediump float;
uniform vec4 color;
void main() { gl_FragColor = vec4(color.rgb * color.a, color.a); })";

GLuint Renderer::compile(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    LOGE("shader compile: %s", log);
  }
  return s;
}

GLuint Renderer::link(const char* vs, const char* fs) {
  GLuint p = glCreateProgram();
  glAttachShader(p, compile(GL_VERTEX_SHADER, vs));
  glAttachShader(p, compile(GL_FRAGMENT_SHADER, fs));
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    glGetProgramInfoLog(p, sizeof(log), nullptr, log);
    LOGE("program link: %s", log);
  }
  return p;
}

bool Renderer::init() {
  tex_prog_ = link(kTexVS, kTexFS);
  solid_prog_ = link(kSolidVS, kSolidFS);
  if (!tex_prog_ || !solid_prog_) return false;

  tex_pos_ = glGetAttribLocation(tex_prog_, "pos");
  tex_uv_ = glGetAttribLocation(tex_prog_, "uv");
  tex_tex_ = glGetUniformLocation(tex_prog_, "tex");
  tex_alpha_ = glGetUniformLocation(tex_prog_, "alpha");
  tex_force_opaque_ = glGetUniformLocation(tex_prog_, "force_opaque");
  tex_transform_ = glGetUniformLocation(tex_prog_, "transform");
  tex_out_size_ = glGetUniformLocation(tex_prog_, "out_size");
  solid_pos_ = glGetAttribLocation(solid_prog_, "pos");
  solid_color_ = glGetUniformLocation(solid_prog_, "color");
  solid_out_size_ = glGetUniformLocation(solid_prog_, "out_size");

  glGenBuffers(1, &vbo_);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  return true;
}

void Renderer::shutdown() {
  if (tex_prog_) glDeleteProgram(tex_prog_);
  if (solid_prog_) glDeleteProgram(solid_prog_);
  if (vbo_) glDeleteBuffers(1, &vbo_);
}

void Renderer::begin(int32_t w, int32_t h) {
  out_w_ = w;
  out_h_ = h;
  glViewport(0, 0, w, h);
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::clear(float r, float g, float b, float a) {
  glClearColor(r, g, b, a);
  glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::texture(GLuint tex, int32_t x, int32_t y, int32_t w, int32_t h,
                       int transform, bool force_opaque, float alpha) {
  if (w <= 0 || h <= 0 || !tex) return;
  float verts[] = {
      (float)x,     (float)y,     0, 0,
      (float)x + w, (float)y,     1, 0,
      (float)x,     (float)y + h, 0, 1,
      (float)x + w, (float)y + h, 1, 1,
  };
  glUseProgram(tex_prog_);
  glUniform2f(tex_out_size_, (float)out_w_, (float)out_h_);
  glUniform1f(tex_alpha_, alpha);
  glUniform1f(tex_force_opaque_, force_opaque ? 1.0f : 0.0f);
  glUniform1i(tex_transform_, transform);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, tex);
  glUniform1i(tex_tex_, 0);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
  glEnableVertexAttribArray(tex_pos_);
  glVertexAttribPointer(tex_pos_, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        nullptr);
  glEnableVertexAttribArray(tex_uv_);
  glVertexAttribPointer(tex_uv_, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        (void*)(2 * sizeof(float)));
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void Renderer::rect(int32_t x, int32_t y, int32_t w, int32_t h, float r,
                    float g, float b, float a) {
  float verts[] = {
      (float)x,     (float)y,
      (float)x + w, (float)y,
      (float)x,     (float)y + h,
      (float)x + w, (float)y + h,
  };
  glUseProgram(solid_prog_);
  glUniform2f(solid_out_size_, (float)out_w_, (float)out_h_);
  glUniform4f(solid_color_, r, g, b, a);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STREAM_DRAW);
  glEnableVertexAttribArray(solid_pos_);
  glVertexAttribPointer(solid_pos_, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

GLuint Renderer::texture_from_pixels(int32_t w, int32_t h, const void* rgba) {
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               rgba);
  return tex;
}

}  // namespace oasis
