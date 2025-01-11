#ifndef __BITTY_CELL_HH__
#define __BITTY_CELL_HH__

#include <boost/container_hash/hash.hpp>
#include <cstdint>
#include <glm/mat4x4.hpp>

namespace bitty {
enum CellFlags : uint16_t {
  kNone = (uint16_t)0,
  kBold = (uint16_t)1,
  kItalic = (uint16_t)2,
  kUnderline = (uint16_t)4,
  kStrikethrough = (uint16_t)8,
  kAll = (uint16_t)(kBold | kItalic | kUnderline | kStrikethrough)
};

union Color {
  struct {
    uint32_t a : 8;
    uint32_t r : 8;
    uint32_t g : 8;
    uint32_t b : 8;
  };

  uint32_t raw;

  inline constexpr Color() : a(0), r(0), g(0), b(0) {}

  inline constexpr Color(uint8_t A, uint8_t R, uint8_t G, uint8_t B)
      : a(A), r(R), g(G), b(B) {}

  inline constexpr explicit Color(uint32_t raw_color) : raw(raw_color) {}
  inline constexpr explicit Color(glm::vec4 vec)
      : a(vec.a * 255.f),
        r(vec.r * 255.f),
        g(vec.g * 255.f),
        b(vec.b * 255.f) {}

  inline constexpr glm::vec4 AsVec4() { return glm::vec4(r, g, b, a) / 255.f; };

  inline constexpr bool operator==(const Color &color) const {
    return raw == color.raw;
  }

  inline constexpr static Color Decode3BitColor(uint32_t bits, int intensity) {
    return Color(255, intensity * !!(bits & 1), intensity * !!(bits & 2),
                 intensity * !!(bits & 4));
  }
};

struct Cell {
  char32_t displayed_code, true_code;
  CellFlags flags;
  uint16_t segment_index;
  uint16_t segment_count;

  inline Cell()
      : displayed_code(0),
        true_code(0),
        flags(CellFlags::kNone),
        segment_index(0),
        segment_count(0) {}

  inline Cell(char32_t ccode, uint16_t cflags, uint16_t seg_index = 0,
              uint16_t seg_count = 1)
      : displayed_code(ccode),
        true_code(ccode),
        flags{cflags},
        segment_index(seg_index),
        segment_count(seg_count) {}

  inline bool operator==(const Cell &character) const {
    return true_code == character.true_code &&
           displayed_code == character.displayed_code &&
           flags == character.flags &&
           segment_count == character.segment_count &&
           segment_index == character.segment_index;
  }
};

struct ColoredCell : public Cell {
  Color foreground, background;

  inline ColoredCell() : Cell(), foreground(0), background(0) {}

  inline ColoredCell SwapColors() {
    auto copy = *this;
    std::swap(copy.foreground, copy.background);
    return copy;
  }

  inline ColoredCell(char32_t ccode, Color fg, Color bg,
                     CellFlags cflags = CellFlags::kNone, uint8_t seg_index = 0,
                     uint8_t seg_count = 1)
      : Cell(ccode, cflags, seg_index, seg_count),
        foreground(fg),
        background(bg) {}

  inline ColoredCell(Cell cell, Color fg, Color bg)
      : Cell(cell), foreground(fg), background(bg) {}

  inline bool operator==(const ColoredCell &character) const {
    return static_cast<const Cell &>(*this) ==
               static_cast<const Cell &>(character) &&
           foreground == character.foreground &&
           background == character.background;
  }
};

}  // namespace bitty

namespace std {
template <>
struct hash<bitty::Cell> {
  size_t operator()(bitty::Cell c) const {
    size_t result = 0;
    boost::hash_combine(result, c.displayed_code);
    boost::hash_combine(result, c.true_code);
    boost::hash_combine(result, c.segment_index);
    boost::hash_combine(result, c.segment_count);
    boost::hash_combine(result, c.flags);
    return result;
  }
};
}  // namespace std

#endif /* __BITTY_CELL_HH__ */