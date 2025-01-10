#ifndef __BITTY_FONTS_HH__
#define __BITTY_FONTS_HH__

#include <freetype2/ft2build.h>
#include <unordered_map>
#include FT_FREETYPE_H

#include <harfbuzz/hb.h>

#include <mutex>

#include "cell.hh"
#include "config.hh"
#include "util.hh"

namespace bitty {
class CharsetBuffer;

class FontRenderer final : public ConfigListener {
  std::mutex mutex_;
  FT_Library library_;
  FT_Face face_normal_, face_bold_;
  i32 cell_width_px_{0}, cell_height_px_{0}, baseline_y_;
  constexpr static size_t kMaxWidthInCellsCacheSize {65536};
  std::unordered_map<char32_t, uint32_t> width_in_cells_cache_;
  FontRenderer();

  FontRenderer(const FontRenderer &) = delete;
  void operator=(const FontRenderer &) = delete;

 public:
  static FontRenderer &Get();

  void OnConfigReload();

  inline u32 CellWidthPx() const { return cell_width_px_; }
  inline u32 CellHeightPx() const { return cell_height_px_; }

  inline u32 FontBaselineY() const { return baseline_y_; }

  u32 GetCodePointWidthInCells(char32_t codepoint);

  bool RenderCharacter(CharsetBuffer &buf, Cell chr, size_t x, size_t y);

  inline ~FontRenderer() { StopListening(); }
};

inline u32 GlobalCellWidthPx() { return FontRenderer::Get().CellWidthPx(); }
inline u32 GlobalCellHeightPx() { return FontRenderer::Get().CellHeightPx(); }

}  // namespace bitty

#endif /* __BITTY_FONTS_HH__ */