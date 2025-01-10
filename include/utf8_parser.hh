#ifndef __UTF8_PARSER_HH__
#define __UTF8_PARSER_HH__

#include <array>

#include "util.hh"

namespace bitty {
struct FirstByteTable {
  std::array<u8, 32> bytes_left;
  std::array<u8, 32> masks;

  constexpr FirstByteTable() {
    for (u8 i = 0; i < 32; i++) {
      if (i == 0b11110) {
        bytes_left[i] = 4;
        masks[i] = 0b111;
      } else if ((i & 0b11110) == 0b11100) {
        bytes_left[i] = 3;
        masks[i] = 0b1111;
      } else if ((i & 0b11100) == 0b11000) {
        bytes_left[i] = 2;
        masks[i] = 0b11111;
      } else {
        bytes_left[i] = 1;
        masks[i] = 0b1111111;
      }
    }
  }
};

class Utf8Parser {
  u32 bytes_left_{0};
  u32 code_point_{0};

  constexpr static FirstByteTable fb_table_ = FirstByteTable();

 public:
  inline u32 Feed(char byte) {
    switch (bytes_left_) {
      case 0: {
        u8 entry = u8(byte) >> 3;
        bytes_left_ = fb_table_.bytes_left[entry];
        code_point_ = byte & fb_table_.masks[entry];
        break;
      }

      case 1:
      case 2:
      case 3:
        code_point_ <<= 6;
        code_point_ |= byte & 0b111111;
        break;
    }

    return --bytes_left_ == 0 ? code_point_ : -1;
  }
};
}  // namespace bitty

#endif /* __UTF8_PARSER_HH__ */
