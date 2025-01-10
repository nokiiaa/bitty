#ifndef __BITTY_SLAB_ALLOC_HH__
#define __BITTY_SLAB_ALLOC_HH__

#include <boost/dynamic_bitset.hpp>
#include <boost/dynamic_bitset/dynamic_bitset.hpp>
#include <concepts>
#include <cstddef>
#include <memory>
#include <optional>

namespace bitty {
template <typename T>
  requires std::integral<T> && (sizeof(T) <= sizeof(size_t))
class IndexAllocator {
  size_t max_count_, tail_, allocated_;
  std::unique_ptr<T[]> values_;
  boost::dynamic_bitset<> allocated_set_;

  void operator=(const IndexAllocator&) = delete;
  IndexAllocator(const IndexAllocator&) = delete;
 public:

  inline IndexAllocator(size_t max_count)
      : max_count_(max_count),
        tail_(0),
        allocated_(0),
        values_(new T[max_count]),
        allocated_set_(max_count) {
    for (T i = 0; i < max_count_; i++) values_[i] = i + 1;
  }

  inline std::optional<T> Allocate() {
    if (allocated_ == max_count_) return std::nullopt;

    auto old_tail = tail_;
    tail_ = values_[tail_];
    allocated_++;
    allocated_set_[old_tail] = true;

    return old_tail;
  }

  inline bool Free(T value) {
    if (allocated_ == 0 || !allocated_set_[value]) return false;

    auto old_tail = tail_;
    tail_ = value;
    values_[tail_] = old_tail;
    allocated_--;
    allocated_set_[value] = false;

    return true;
  }
};
}  // namespace bitty

#endif /* __BITTY_SLAB_ALLOC_HH__ */