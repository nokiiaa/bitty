#ifndef __BITTY_BUF_RENDERER_HH__
#define __BITTY_BUF_RENDERER_HH__

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "charset.hh"
#include "gl_program.hh"
#include "terminal.hh"

namespace bitty {
class CellBuffer;

struct VertexBufElement {
  glm::vec4 position;
  glm::vec2 uv;
  glm::vec4 foreground, background;
};

class TermRenderer {
  std::vector<VertexBufElement> vbo_data_;
  std::vector<u32> ibo_data_;
  GLProgram buf_program_, cursor_program_;
  GLuint pos_loc_, uv_loc_, fore_loc_, back_loc_, vbo_, ibo_, vao_;
  Charset charset_;

  bool SetupGLBuffers();
  void AllocateBufferData();

 public:
  TermRenderer();

  bool Render(Terminal &term, uint32_t window_width, uint32_t window_height);
};
}  // namespace bitty

#endif /* __BITTY_BUF_RENDERER_HH__ */