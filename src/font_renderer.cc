#include "font_renderer.hh"

#include <fontconfig/fontconfig.h>
#include <freetype/freetype.h>
#include <freetype/ftimage.h>
#include <freetype/fttypes.h>

#include <cmath>
#include <mutex>
#include <stdexcept>

#include "cell_buffer.hh"
#include "charset.hh"
#include "util.hh"

#include FT_LCD_FILTER_H

namespace bitty {
FontRenderer::FontRenderer() {
  FT_Error error{FT_Init_FreeType(&library_)};

  if (error) throw std::runtime_error("Failed to initialize FreeType");

  OnConfigReload();
}

void FontRenderer::OnConfigReload() {
  std::unique_lock lock{mutex_};

  const auto& conf = Config::Get();

  std::string font_family = conf.FontFamily().value_or("monospace");

  double font_pt = conf.FontSize() * conf.CalcPixelsPerPt() * 1.25;

  for (const auto& [is_bold, face] :
       {std::tuple{false, &face_normal_}, std::tuple{true, &face_bold_}}) {
    FcConfig* fc_config = FcInitLoadConfigAndFonts();

    auto ff = font_family;

    FcPattern* pat = FcNameParse((const FcChar8*)ff.c_str());

    FcPatternAddInteger(pat, FC_WEIGHT,
                        is_bold ? FC_WEIGHT_BOLD : FC_WEIGHT_MEDIUM);

    FcConfigSubstitute(fc_config, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult res;
    FcPattern* font;

    FcFontSet* fs = FcFontSetCreate();
    FcObjectSet* os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FILE, (char*)0);

    FcFontSet* font_patterns;
    font_patterns = FcFontSort(fc_config, pat, FcTrue, 0, &res);

    if (!font_patterns || font_patterns->nfont == 0)
      throw std::runtime_error{
          "Fontconfig could not find any fonts on the system\n"};

    FcPattern* font_pattern;

    if ((font_pattern =
             FcFontRenderPrepare(fc_config, pat, font_patterns->fonts[0])))
      FcFontSetAdd(fs, font_pattern);
    else
      throw std::runtime_error{"Could not prepare matched font for loading.\n"};

    const char* font_file = nullptr;

    FcValue v;

    if (fs && fs->nfont > 0) {
      font = FcPatternFilter(fs->fonts[0], os);

      FcPatternGet(font, FC_FILE, 0, &v);
      font_file = (char*)v.u.f;
    } else
      throw std::runtime_error("Could not obtain fs\n");

    ScopeGuard sg([&] {
      FcFontSetSortDestroy(font_patterns);
      FcPatternDestroy(font_pattern);
      FcPatternDestroy(pat);
      FcFontSetDestroy(fs);
      FcPatternDestroy(font);
      FcConfigDestroy(fc_config);
    });

    LogInfo() << "Found " << font_file << " for " << ff << '\n';

    FT_Error error{FT_New_Face(library_, font_file, 0, face)};

    if (error) throw std::runtime_error("Error initializing face");

    if (is_bold && !((*face)->style_flags & FT_STYLE_FLAG_BOLD))
      LogWarning() << "Failed to find bold typeface for " << ff << '\n';

    if ((error = FT_Set_Char_Size(*face, 0, font_pt * 64.0, 0, 0)))
      throw std::runtime_error("Error setting character size");
  }

  cell_width_px_ = 0;
  baseline_y_ = CeilFrom266(face_normal_->size->metrics.ascender);
  cell_height_px_ = CeilFrom266(face_normal_->size->metrics.height);

  i32 cell_width_266 = 0;

  auto check_glyph = [&](char32_t codepoint,
                         bool adjust_width = false) mutable {
    FT_UInt glyph_index = FT_Get_Char_Index(face_normal_, codepoint);

    FT_Error error{
        FT_Load_Glyph(face_normal_, glyph_index, FT_LOAD_TARGET_LIGHT)};

    if (!error) {
      if (adjust_width) {
        cell_width_266 = std::max(
            cell_width_266, (i32)face_normal_->glyph->metrics.horiAdvance);
      }
    }
  };

  for (uint32_t i = 0x21; i < 0x80; i++) check_glyph(i, true);

  cell_width_px_ = CeilFrom266(cell_width_266);
}

bool FontRenderer::RenderCharacter(CharsetBuffer& buf, Cell chr, size_t x,
                                   size_t y) {
  FT_Face face;

  bool bold = chr.flags & CellFlags::kBold;

  if (bold)
    face = face_bold_;
  else
    face = face_normal_;

  std::unique_lock lock{mutex_};

  FT_UInt glyph_index{FT_Get_Char_Index(face, chr.displayed_code)};

  FT_Error error{FT_Load_Glyph(face, glyph_index, FT_LOAD_TARGET_LIGHT)};

  if (error) return false;

  FT_GlyphSlot slot{face->glyph};

  if ((error = FT_Render_Glyph(slot, FT_RENDER_MODE_LIGHT))) return false;

  return buf.Render(slot, x, y, chr);
}

u32 FontRenderer::GetCodePointWidthInCells(char32_t codepoint) {
  if (auto it = width_in_cells_cache_.find(codepoint);
      it != width_in_cells_cache_.end())
    return it->second;

  FT_UInt glyph_index{FT_Get_Char_Index(face_normal_, codepoint)};

  FT_Load_Glyph(face_normal_, glyph_index, FT_LOAD_TARGET_LIGHT);

  u32 pixels = std::abs(CeilFrom266(face_normal_->glyph->metrics.width)) +
               std::abs(CeilFrom266(face_normal_->glyph->metrics.horiBearingX));

  u32 w = CellWidthPx();

  if (width_in_cells_cache_.size() >= kMaxWidthInCellsCacheSize)
    width_in_cells_cache_.erase(width_in_cells_cache_.begin());

  return width_in_cells_cache_[codepoint] =
             std::max(0, i32(pixels + w - 3)) / w;
}

FontRenderer& FontRenderer::Get() {
  static thread_local FontRenderer renderer;
  return renderer;
}
}  // namespace bitty
