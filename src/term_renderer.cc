#include "term_renderer.hh"

#include <glad/gl.h>

#include <glm/ext.hpp>
#include <glm/ext/matrix_float3x2.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/mat3x2.hpp>

#include "cell_buffer.hh"
#include "font_renderer.hh"
#include "terminal.hh"
#include "util.hh"

namespace bitty {
bool TermRenderer::SetupGLBuffers() {
  glGenBuffers(1, &vbo_);

  glGenBuffers(1, &ibo_);

  glGenVertexArrays(1, &vao_);

  glBindVertexArray(vao_);

  pos_loc_ = 0;
  uv_loc_ = 1;
  fore_loc_ = 2;
  back_loc_ = 3;

  glEnableVertexAttribArray(pos_loc_);

  glEnableVertexAttribArray(uv_loc_);

  glEnableVertexAttribArray(fore_loc_);

  glEnableVertexAttribArray(back_loc_);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);

  float *ptr = nullptr;

  for (const auto [loc, size] :
       {std::tuple{pos_loc_, 4}, std::tuple{uv_loc_, 2},
        std::tuple{fore_loc_, 4}, std::tuple{back_loc_, 4}}) {
    glVertexAttribPointer(loc, size, GL_FLOAT, GL_FALSE,
                          sizeof(VertexBufElement), ptr);

    ptr += size;
  }

  return true;
}

TermRenderer::TermRenderer()
    : buf_program_(GLProgram::FromFiles("shaders/buf_vertex.glsl",
                                        "shaders/buf_fragment.glsl")),
      cursor_program_(GLProgram::FromFiles("shaders/cursor_vertex.glsl",
                                           "shaders/cursor_fragment.glsl")),
      charset_(128, 128) {
  SetupGLBuffers();
}

bool TermRenderer::Render(Terminal &term, u32 window_width, u32 window_height) {
  auto ch_w = GlobalCellWidthPx();
  auto ch_h = GlobalCellHeightPx();

  std::shared_ptr<CellBuffer> buf = term.CurrentBuffer();

  size_t w = buf->Width(), h = buf->VisibleHeight();
  size_t buf_wh = w * h;

  ibo_data_ = std::vector<u32>();
  ibo_data_.reserve(buf_wh * 6);

  if (vbo_data_.size() < buf_wh * 4)
    vbo_data_ = std::vector<VertexBufElement>(buf_wh * 4);

  glm::dvec2 window_size(window_width, window_height);

  auto id = glm::dmat4(1);

  glm::dmat4 xy_to_normalized = glm::scale(id, glm::dvec3(1, -1, 1)) *
                                glm::translate(id, glm::dvec3(-1, -1, 0)) *
                                glm::scale(id, glm::dvec3(2. / window_size, 1));

  auto wh =
      glm::vec2(charset_.TexWidthInPixels(), charset_.TexHeightInPixels());

  auto opacity_vec = glm::vec4(1, 1, 1, bitty::Config::Get().Opacity());

  ColoredCell org_cell_at_cursor{};
  bool cursor_was_displayed = false;

  bool show_cursor = term.IsCursorVisible() && !buf->UserScrolledUp();

  if (show_cursor) {
    if (auto cell_at_curs = buf->Get(term.CursorX(), term.CursorY());
        cell_at_curs.has_value()) {
      org_cell_at_cursor = cell_at_curs.value();

      buf->Set(term.CursorX(), term.CursorY(), org_cell_at_cursor.SwapColors());
      cursor_was_displayed = true;
    }
  }

  auto add_char_to_buffer = [&](u32 x, u32 y, ColoredCell chr) mutable -> bool {
    TexRegion<u32> region = charset_.MapCharacter(chr);

    auto tl = glm::vec2(region.top_left);
    auto br = glm::vec2(region.bottom_right);

    auto tl_gl = tl / wh;
    auto br_gl = br / wh;

    auto tr_gl = glm::vec2(br_gl.x, tl_gl.y);
    auto bl_gl = glm::vec2(tl_gl.x, br_gl.y);

    auto sx = x * ch_w;
    auto sy = y * ch_h;

    u32 vert_base = 4 * (x + y * w);

    for (const auto [i, xy, uv] : {
             std::tuple{0, glm::vec2(sx, sy), tl_gl},
             {1, glm::vec2(sx, sy + ch_h), bl_gl},
             {2, glm::vec2(sx + ch_w, sy + ch_h), br_gl},
             {3, glm::vec2(sx + ch_w, sy), tr_gl},
         }) {
      auto vertex_pos = xy_to_normalized * glm::dvec4(xy, 0.0, 1.0);

      bool is_default_bg_color = chr.background.r == 0 &&
                                 chr.background.g == 0 && chr.background.b == 0;

      vbo_data_[i + vert_base] = VertexBufElement{
          .position = (glm::vec4)vertex_pos,
          .uv = uv,
          .foreground = chr.foreground.AsVec4(),
          .background = chr.background.AsVec4() *
                        (is_default_bg_color ? opacity_vec : glm::vec4(1))};
    }

    return true;
  };

  buf->ProcessUpdates(add_char_to_buffer);
  buf->EnumerateNonEmptyCells([&](u32 idx) -> bool {
    idx *= 4;

    ibo_data_.insert(ibo_data_.end(),
                     {idx + 0, idx + 1, idx + 3, idx + 1, idx + 2, idx + 3});

    return true;
  });

  if (cursor_was_displayed)
    buf->Set(term.CursorX(), term.CursorY(), org_cell_at_cursor);

  charset_.UploadToGL();

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_);

  glBufferData(GL_ELEMENT_ARRAY_BUFFER, ibo_data_.size() * sizeof(u32),
               ibo_data_.data(), GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, vbo_);

  glBufferData(GL_ARRAY_BUFFER, vbo_data_.size() * sizeof(VertexBufElement),
               vbo_data_.data(), GL_DYNAMIC_DRAW);

  buf_program_.Use();
  buf_program_.SetUniform("transform",
                          glm::mat4(xy_to_normalized * buf->GetTransform() *
                                    glm::inverse(xy_to_normalized)));

  buf_program_.SetUniform<GLint>("cell_width", ch_w);

  glBindTexture(GL_TEXTURE_2D, charset_.GetGLTexture());
  glBindVertexArray(vao_);

  glDrawElements(GL_TRIANGLES, ibo_data_.size(), GL_UNSIGNED_INT, 0);

  return true;
}
}  // namespace bitty