#ifndef __BITTY_TEX_COORD_HH__
#define __BITTY_TEX_COORD_HH__

#include <cstddef>
#include <cstdint>
#include <boost/container_hash/hash.hpp>

#include <glm/vec2.hpp>

namespace bitty {
template <typename T>
struct TexCoord {
  T x, y;

  operator glm::vec2() { return glm::vec2(x, y); };
};

template <typename T>
struct TexRegion {
  TexCoord<T> top_left, bottom_right;
};
}  // namespace bitty


namespace std {
template <>
struct hash<bitty::TexCoord<uint32_t>> {
  size_t operator()(bitty::TexCoord<uint32_t> c) const {
    if constexpr (sizeof(size_t) == 8)
      return c.x | size_t(c.y) << 32;
    else {
      size_t result = 0;
      boost::hash_combine(result, c.x);
      boost::hash_combine(result, c.y);
      return result;
    }
  }
};
}  // namespace std

#endif /* __BITTY_TEX_COORD_HH__ */