#include "cell_buffer.hh"

#include <boost/dynamic_bitset/dynamic_bitset.hpp>

#include "cell.hh"
#include "font_renderer.hh"

namespace bitty {

bool CellBuffer::UserScrolledUp() const {
  return UserScrollInCells() != ScrollInCells();
}

void CellBuffer::UserScrollByNPixels(i32 n) {
  user_scroll_in_pixels_ =
      std::min(i32(HistorySizeInCells() * GlobalCellHeightPx()),
               std::max(0, user_scroll_in_pixels_ + n));
  dirty_mask_.set(0, dirty_mask_.size(), 1);
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

    data_.resize(width_ * height_);
  }

  if (!UserScrolledUp())
    UserScrollByNPixels(n * (i32)GlobalCellHeightPx());

  scroll_in_cells_ = new_scroll_in_cells;
}

void CellBuffer::ResetUserScroll() {
  user_scroll_in_pixels_ = scroll_in_cells_ * GlobalCellHeightPx();
  dirty_mask_.set(0, dirty_mask_.size(), 1);
}

void CellBuffer::ResetScroll() {
  scroll_in_cells_ = HistorySizeInCells();
}

bool CellBuffer::CopyArea(Rect<u32> src, Rect<u32> dest) {
  if (!src.IsValid() || !dest.IsValid()) return false;

  Rect<u32> buf_rect = {0, 0, Width(), VisibleHeight()};

  dest.Clamp(buf_rect);
  src.CopyWidthAndHeight(dest);
  src.Clamp(buf_rect);

  size_t offset = width_ * ScrollInCells();

  ColoredCell *base = data_.data() + offset;

  u32 buf_w = Width();

  u32 w = src.Width(), h = src.Height();
  size_t size_of_row = w * sizeof(ColoredCell);

  if (!src.IsValid() || !dest.IsValid() || src.Width() != dest.Width() ||
      src.Height() != dest.Height())
    return false;

  for (u32 y = 0; y < h; y++)
    dirty_mask_.set(dest.left + buf_w * (dest.top + y), w, 1);

  if (src.top > dest.top) {
    // Copy goes from top to bottom
    for (u32 y = 0; y < h; y++) {
      std::memmove(base + dest.left + buf_w * (dest.top + y),
                   base + src.left + buf_w * (src.top + y), size_of_row);
    }
  } else {
    // Copy goes from bottom to top
    for (u32 y = 0; y < h; y++)
      std::memmove(base + dest.left + buf_w * (dest.bottom - y - 1),
                   base + src.left + buf_w * (src.bottom - y - 1), size_of_row);
  }

  return true;
}

bool CellBuffer::FillLine(u32 left, u32 right, u32 y, ColoredCell value) {
  right = std::min(width_, right);
  if (left > right) return false;
  if (y >= VisibleHeight()) return false;

  size_t offset = width_ * (y + ScrollInCells());

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

  u32 buf_w = Width();

  size_t offset = width_ * ScrollInCells();

  ColoredCell *base = data_.data() + offset;

  for (u32 y = area.top; y < area.bottom; y++) {
    for (u32 x = area.left; x < area.right; x++) base[x + buf_w * y] = value;

    dirty_mask_.set(area.left + buf_w * y, area.right - area.left, 1);
  }

  return true;
}

void CellBuffer::ProcessUpdates(
    std::function<bool(u32, u32, ColoredCell)> func) {
  size_t updated = dirty_mask_.find_first();

  u32 x = 0, y = 0;

  u32 scroll = UserScrollInCells();

  while (updated != boost::dynamic_bitset<>::npos) {
    x = updated % Width();
    y = updated / Width();

    if (y + scroll >= height_) break;

    if (const auto &cell = data_.at(updated + scroll * Width());
        cell.displayed_code)
      func(x, y, cell);

    updated = dirty_mask_.find_next(updated);
  }

  ResetUpdates();
}

void CellBuffer::EnumerateNonEmptyCells(std::function<bool(u32)> func) {
  for (u32 i = UserScrollInCells() * Width(), j = VisibleHeight() * Width(),
           k = 0;
       k < j; k++)
    if (data_[i + k].displayed_code) func(k);
}
}  // namespace bitty