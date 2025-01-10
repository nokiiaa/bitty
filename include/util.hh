#ifndef __BITTY_UTIL_HH__
#define __BITTY_UTIL_HH__

#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>

#include "cell.hh"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

namespace bitty {
template <typename T = u32>

struct Rect {
  T left, top, right, bottom;

  inline void Clamp(Rect to) {
    left = std::max(left, to.left);
    right = std::min(right, to.right);
    top = std::max(top, to.top);
    bottom = std::min(bottom, to.bottom);
  }

  inline void CopyWidthAndHeight(Rect from) {
    right = left + from.right - from.left;
    bottom = top + from.bottom - from.top;
  }

  inline bool IsValid() { return right >= left && bottom >= top; }

  inline T Width() { return right - left; }

  inline T Height() { return bottom - top; }

  inline bool operator==(const Rect &rect) const = default;
};

template <typename... Ts>
struct Overloaded : Ts... {
  using Ts::operator()...;
};

template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

void EnableGLDebugOutput();
inline std::ostream &LogInfo() {
  return std::cout << "\e[94m"
                      "[info] "
                      "\e[0m";
}

inline std::ostream &LogWarning() {
  return std::cout << "\e[97m"
                      "[warn] "
                      "\e[0m";
}

inline std::ostream &LogError() {
  return std::cout << "\e[91m"
                      "[err] "
                      "\e[0m";
}

inline i32 CeilFrom266(i32 pos) { return (pos + 64 - 1) / 64; }
inline i32 RoundFrom266(i32 pos) { return (pos + 32 - 1) / 64; }
inline i32 FloorFrom266(i32 pos) { return pos / 64; }
inline i32 CeilFrom1616(i32 pos) { return (pos + 65536 - 1) / 65536; }
inline i32 RoundFrom1616(i32 pos) { return (pos + 32768 - 1) / 65536; }
inline i32 FloorFrom1616(i32 pos) { return pos / 65536; }

template<typename T>
inline T CeilDiv(T a, T b) {
  return (a + b - 1) / b;
}

template <typename T, typename U>
  requires(std::is_signed_v<T> && std::is_unsigned_v<U>)
inline T EuclideanMod(T a, U b) {
  T r = a % b;
  return r + T((r < 0) * b);
}


template <typename T>
class ScopeGuard {
  T deleter_;

 public:
  ScopeGuard(T d) : deleter_(d) {}
  ~ScopeGuard() { deleter_(); }
};

}  // namespace bitty

#endif /* __BITTY_UTIL_HH__ */