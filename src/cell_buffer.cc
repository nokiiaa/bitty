#include "cell_buffer.hh"

#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <stdexcept>

#include "cell.hh"
#include "font_renderer.hh"
#include "util.hh"

namespace bitty {

bool CellBuffer::UserScrolledUp() const {
  return UserScrollInCells() != ScrollInCells();
}

  void CellBuffer::MarkAllAsDirty() {
  dirty_mask_.set();

  }

void CellBuffer::UserScrollByNPixels(i32 n) {
  user_scroll_in_pixels_ =
      std::min(i32(HistorySizeInCells() * GlobalCellHeightPx()),
               std::max(0, user_scroll_in_pixels_ + n));
  MarkAllAsDirty();
}

void CellBuffer::ScrollByNCells(i32 n, bool allow_buf_expansion) {
  u32 new_scroll_in_cells = std::max(0, scroll_in_cells_ + n);

  if (!allow_buf_expansion && new_scroll_in_cells > HistorySizeInCells()) {
    ResetScroll();
    if (!UserScrolledUp()) ResetUserScroll();
    return;
  }

  i32 added_cells = i32(new_scroll_in_cells - HistorySizeInCells());

  if (added_cells != 0) {
    height_ += added_cells;

    data_.resize(pitch_ * height_);
  }

  if (!UserScrolledUp()) UserScrollByNPixels(n * (i32)GlobalCellHeightPx());

  scroll_in_cells_ = new_scroll_in_cells;
}

void CellBuffer::ResetUserScroll() {
  user_scroll_in_pixels_ = scroll_in_cells_ * GlobalCellHeightPx();
  MarkAllAsDirty();
}

void CellBuffer::ResetScroll() { scroll_in_cells_ = HistorySizeInCells(); }

bool CellBuffer::CopyArea(Rect<u32> src, Rect<u32> dest) {
  if (!src.IsValid() || !dest.IsValid()) return false;

  Rect<u32> buf_rect = {0, 0, Width(), VisibleHeight()};

  dest.Clamp(buf_rect);
  src.CopyWidthAndHeight(dest);
  src.Clamp(buf_rect);

  size_t offset = pitch_ * ScrollInCells();

  ColoredCell *base = data_.data() + offset;

  u32 w = src.Width(), h = src.Height();
  size_t size_of_row = w * sizeof(ColoredCell);

  if (!src.IsValid() || !dest.IsValid() || src.Width() != dest.Width() ||
      src.Height() != dest.Height())
    return false;

  for (u32 y = 0; y < h; y++)
    dirty_mask_.set(dest.left + width_ * (dest.top + y), w, 1);

  if (src.top > dest.top) {
    // Copy goes from top to bottom
    for (u32 y = 0; y < h; y++) {
      std::memmove(base + dest.left + pitch_ * (dest.top + y),
                   base + src.left + pitch_ * (src.top + y), size_of_row);
    }
  } else {
    // Copy goes from bottom to top
    for (u32 y = 0; y < h; y++)
      std::memmove(base + dest.left + pitch_ * (dest.bottom - y - 1),
                   base + src.left + pitch_ * (src.bottom - y - 1),
                   size_of_row);
  }

  return true;
}

bool CellBuffer::FillLine(u32 left, u32 right, u32 y, ColoredCell value) {
  right = std::min(width_, right);
  if (left > right) return false;
  if (y >= VisibleHeight()) return false;

  size_t offset = pitch_ * (y + ScrollInCells());

  ColoredCell *base = data_.data() + offset;

  dirty_mask_.set(y * width_ + left, right - left, 1);

  for (u32 x = left; x < right; x++) base[x] = value;

  return true;
}

void CellBuffer::ResetUpdates() { dirty_mask_.reset(); }

bool CellBuffer::FillArea(Rect<u32> area, ColoredCell value) {
  if (!area.IsValid()) return false;

  Rect<u32> buf_rect = {0, 0, Width(), VisibleHeight()};
  area.Clamp(buf_rect);

  if (!buf_rect.IsValid()) return false;

  size_t offset = pitch_ * ScrollInCells();

  ColoredCell *base = data_.data() + offset;

  for (u32 y = area.top; y < area.bottom; y++) {
    for (u32 x = area.left; x < area.right; x++) base[x + pitch_ * y] = value;

    dirty_mask_.set(area.left + width_ * y, area.right - area.left, 1);
  }

  return true;
}

void CellBuffer::ProcessUpdates(
    std::function<bool(u32, u32, ColoredCell)> func) {
  size_t updated = dirty_mask_.find_first();

  u32 x = 0, y = 0;

  u32 scroll = UserScrollInCells();

  while (updated != boost::dynamic_bitset<>::npos) {
    x = updated % width_;
    y = updated / width_;

    if (y + scroll >= height_) break;

    if (const auto &cell = data_.at(x + y * pitch_ + scroll * pitch_);
        cell.displayed_code)
      func(x, y, cell);

    updated = dirty_mask_.find_next(updated);
  }

  ResetUpdates();
}

void CellBuffer::EnumerateNonEmptyCells(std::function<bool(u32)> func) {
  for (u32 i = UserScrollInCells(), j = VisibleHeight(), k = 0; k < j; k++)
    for (u32 x = 0; x < Width(); x++)
      if (data_.at((i + k) * pitch_ + x).displayed_code) func(k * width_ + x);
}

std::pair<i32, i32> CellBuffer::Resize(u32 width, u32 height) {
  if (width == width_ && height == height_) return {0, 0};

  if (width == 0 || height == 0)
    throw std::runtime_error("Invalid width or height when resizing");

  u32 old_p = pitch_;
  i32 delta_w = width - width_;
  i32 delta_vh = height - visible_height_;

  width_ = width;

  if (delta_vh > 0 && HistorySizeInCells() == ScrollInCells())
    height_ += delta_vh;

  visible_height_ = height;

  ScrollByNCells(-delta_vh, false);

  if (width_ > pitch_) pitch_ = ExpGrowSize(width_);

  data_.resize(pitch_ * height_);
  dirty_mask_.resize(width_ * visible_height_);

  if (width_ > pitch_) {
    for (u32 h = 0; h < height_; h++) {
      u32 y = height_ - 1 - h;

      std::memmove(data_.data() + y * pitch_, data_.data() + y * old_p,
                   width_ * sizeof(ColoredCell));
    }
  }

  MarkAllAsDirty();

  return {delta_w, delta_vh};
}
}  // namespace bitty
