#include "charset.hh"

#include <GL/glext.h>
#include <freetype/ftimage.h>
#include <glad/gl.h>

#include "cell_buffer.hh"
#include "font_renderer.hh"
#include "util.hh"

namespace bitty {
bool CharsetBuffer::Render(FT_GlyphSlot glyph, i32 x, i32 y, Cell chr) {
  FT_Bitmap *bmp = &glyph->bitmap;

  const auto &renderer = FontRenderer::Get();

  i32 x_offset = glyph->bitmap_left;

  i32 y_offset =
      renderer.FontBaselineY() -
      glyph->bitmap_top;  // renderer.FontBaselineY() -
                          // FloorFrom266(glyph->metrics.horiBearingY);

  enum { R = 0, G, B, kBytesPerPixel = 1 };
  u32 actual_width = bmp->width / kBytesPerPixel;

  u32 segment_offset = chr.segment_index * renderer.CellWidthPx();

  i32 right_border = x + renderer.CellWidthPx();
  i32 bottom_border = y + renderer.CellHeightPx();

  for (u32 Y = 0; Y < bmp->rows; Y++) {
    for (u32 X = 0; X < actual_width; X++) {
      if (auto x_inside_bmp = X + segment_offset; x_inside_bmp < actual_width) {
        uint8_t *pix = (uint8_t *)bmp->buffer + x_inside_bmp * kBytesPerPixel +
                       bmp->pitch * Y;

        i32 total_x = X + x + x_offset;
        i32 total_y = Y + y + y_offset;

        if (total_x >= x && total_y >= y && total_x < right_border &&
            total_y < bottom_border) {
          uint8_t chan = pix[0];
          auto value = chan << 24 | chan << 16 | chan << 8 | chan;

          buffer_[total_x + total_y * width_px_] = value;
        }
      }
    }
  }

  return true;
}

bool Charset::CreateGLTexture() {
  glGenTextures(1, &texture_id_);

  texture_valid_ = true;

  return true;
}

bool Charset::UploadToGL() {
  if (!texture_valid_) CreateGLTexture();

  if (changes_pending_upload_) {
    glBindTexture(GL_TEXTURE_2D, texture_id_);

    const auto &renderer = FontRenderer::Get();

    size_t tex_width = width_in_chars_ * renderer.CellWidthPx();
    size_t tex_height = height_in_chars_ * renderer.CellHeightPx();

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_width, tex_height, 0, GL_BGRA,
                 GL_UNSIGNED_BYTE, buffer_.Pixels());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float color[4] = {};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, color);
  }

  changes_pending_upload_ = false;

  return true;
}

TexRegion<u32> Charset::MapCharacter(Cell chr) {
  if (auto found = char_map_.find(chr); found != char_map_.end())
    return found->second;

  auto &renderer = FontRenderer::Get();

  u32 idx = char_allocator_.Allocate().value_or(-1u);

  if (idx == -1u) return TexRegion<u32>{{0u, 0u}, {0u, 0u}};

  u32 x_in_chars = idx % width_in_chars_;
  u32 y_in_chars = idx / width_in_chars_;

  u32 x = x_in_chars * renderer.CellWidthPx(),
      y = y_in_chars * renderer.CellHeightPx();

  renderer.RenderCharacter(buffer_, chr, x, y);
  changes_pending_upload_ = true;

  auto region = char_map_[chr] = TexRegion<u32>{
      {x, y}, {x + renderer.CellWidthPx(), y + renderer.CellHeightPx()}};

  for (uint16_t seg = 0; seg < chr.segment_count; seg++)
    if (seg != chr.segment_index)
      MapCharacter(Cell(chr.displayed_code, chr.flags, seg, chr.segment_count));

  return region;
}
}  // namespace bitty