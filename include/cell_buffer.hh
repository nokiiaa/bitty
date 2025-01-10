#ifndef __BITTY_CHAR_BUFFER_HH__
#define __BITTY_CHAR_BUFFER_HH__

#include <boost/container_hash/hash.hpp>
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <cstdint>
#include <functional>
#include <glm/mat4x4.hpp>
#include <optional>
#include <stdexcept>
#include <vector>

#include "cell.hh"
#include "font_renderer.hh"
#include "terminal.hh"
#include "util.hh"

namespace bitty {

class CellBuffer {
  std::vector<ColoredCell> data_;
  u32 width_, height_, visible_height_;
  boost::dynamic_bitset<> dirty_mask_;

  glm::mat4 transform_;

  i32 user_scroll_in_pixels_{0};
  i32 scroll_in_cells_{0};

  CellBuffer(const CellBuffer &buf) = delete;
  void operator=(const CellBuffer &buf) = delete;

  void ResetUpdates();

 public:
  inline CellBuffer(u32 width, u32 height, u32 visible_height)
      : width_(width),
        height_(height),
        visible_height_(visible_height),
        transform_(1) {
    data_ = std::vector<ColoredCell>(width_ * height_);
    dirty_mask_ = boost::dynamic_bitset{data_.size()};
  }

  inline u32 UserScrollInCells() const {
    return CeilDiv(user_scroll_in_pixels_, (i32)GlobalCellHeightPx());
  }

  inline u32 ScrollInCells() const { return scroll_in_cells_; }

  inline std::optional<ColoredCell> Get(u32 x, u32 y,
                                        bool use_user_scroll = false) const {
    y += use_user_scroll ? UserScrollInCells() : ScrollInCells();

    return x < width_ && y < height_ ? std::optional{data_[x + width_ * y]}
                                     : std::nullopt;
  }

  inline bool Set(u32 x, u32 y, ColoredCell chr, bool use_user_scroll = false) {
    u32 Y = y + (use_user_scroll ? UserScrollInCells() : ScrollInCells());

    if (x < width_ && y < height_) {
      data_[x + width_ * Y] = chr;
      dirty_mask_[x + width_ * y] = 1;
      return true;
    }

    return false;
  }

  inline glm::mat4 GetTransform() const { return transform_; }

  inline void SetTransform(glm::mat4 transform) { transform_ = transform; }

  void ProcessUpdates(std::function<bool(u32, u32, ColoredCell)> func);
  void EnumerateNonEmptyCells(std::function<bool(u32)> func);

  inline u32 Width() const { return width_; }
  inline u32 VisibleHeight() const { return visible_height_; }
  inline u32 Height() const { return height_; }

  inline u32 ScreenWidth() const {
    return FontRenderer::Get().CellWidthPx() * width_;
  }

  inline u32 ScreenHeight() const {
    return FontRenderer::Get().CellHeightPx() * visible_height_;
  }

  void ScrollByNCells(i32 n, bool allow_buf_expansion);

  inline u32 HistorySizeInCells() const { return Height() - VisibleHeight(); }

  bool UserScrolledUp() const;
  void UserScrollByNPixels(i32 n);
  void ResetUserScroll();
  void ResetScroll();

  bool CopyArea(Rect<u32> src, Rect<u32> dest);
  bool FillLine(u32 left, u32 right, u32 y, ColoredCell value);
  bool FillArea(Rect<u32> area, ColoredCell value);

  inline void Resize(u32 width, u32 height) {
    if (width_ != width || height_ != height)
      throw std::runtime_error("Resize unimplemented");
  }
};
}  // namespace bitty

#endif /* __BITTY_CHAR_BUFFER_HH__ */
