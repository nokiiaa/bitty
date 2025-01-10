#ifndef __BITTY_IMAGE_BUFFER_HH__
#define __BITTY_IMAGE_BUFFER_HH__

#include <glad/gl.h>

#include <boost/container_hash/hash.hpp>
#include <cstdint>
#include <memory>

#include "font_renderer.hh"
#include "index_alloc.hh"
#include "tex_coord.hh"

namespace bitty {

class CharsetBuffer final {
  std::unique_ptr<u32[]> buffer_;
  size_t width_px_, height_px_;

 public:
  inline CharsetBuffer() : buffer_(nullptr), width_px_(0), height_px_(0) {}
  inline CharsetBuffer(size_t width_px, size_t height_px)
      : buffer_(new u32[width_px * height_px]),
        width_px_(width_px),
        height_px_(height_px) {
    std::memset(buffer_.get(), 0, width_px * height_px * sizeof(u32));
  }

  bool Render(FT_GlyphSlot glyph, i32 x, i32 y, Cell chr);

  inline u32 *Pixels() { return buffer_.get(); }
  inline size_t WidthPx() const { return width_px_; }
  inline size_t HeightPx() const { return height_px_; }
};

class Charset final {
  bool texture_valid_{false};
  GLuint texture_id_{0};

  bool changes_pending_upload_{false};

  std::unordered_map<Cell, TexRegion<u32>> char_map_;

  IndexAllocator<u32> char_allocator_;

  size_t width_in_chars_, height_in_chars_;

  CharsetBuffer buffer_;

  inline void Reset(size_t width_in_chars, size_t height_in_chars) {
    const auto &renderer = FontRenderer::Get();

    buffer_ = CharsetBuffer(renderer.CellWidthPx() * width_in_chars,
                            renderer.CellHeightPx() * height_in_chars);
  }

  bool CreateGLTexture();

 public:
  Charset(size_t width_in_chars, size_t height_in_chars)
      : char_allocator_(width_in_chars * height_in_chars),
        width_in_chars_(width_in_chars),
        height_in_chars_(height_in_chars) {
    CreateGLTexture();
    Reset(width_in_chars, height_in_chars);
  }

  TexRegion<u32> MapCharacter(Cell chr);

  inline GLint GetGLTexture() {
    if (!texture_valid_) return -1;

    return texture_id_;
  }

  inline u32 WidthInChars() const { return width_in_chars_; }
  inline u32 HeightInChars() const { return height_in_chars_; }

  inline u32 TexWidthInPixels() const { return buffer_.WidthPx(); }
  inline u32 TexHeightInPixels() const { return buffer_.HeightPx(); }

  bool UploadToGL();
};
}  // namespace bitty

#endif /* __BITTY_IMAGE_BUFFER_HH__ */