#ifndef __BITTY_EVENTS_HH__
#define __BITTY_EVENTS_HH__

#include <condition_variable>
#include <functional>
#include <queue>
#include <variant>

namespace bitty {
struct EventMouseScroll {
  double offset_x, offset_y;
};

struct EventMouseButton {
  int button, action, mods;
};

struct EventMousePos {
  double new_pos_x, new_pos_y;
};

struct EventKeyInput {
  int key, scancode, action, mods;
};

struct EventCharInput {
  char32_t code;
};

struct EventWindowResized {
  int new_width, new_height;
};

struct EventDataFromTty {
  int terminal_id;
  std::unique_ptr<std::byte[]> bytes;
  size_t byte_count;
};

using Event = std::variant<EventMouseScroll, EventMouseButton, EventMousePos,
                           EventKeyInput, EventCharInput, EventWindowResized,
                           EventDataFromTty>;

class EventQueue {
  std::mutex mutex_;
  std::queue<Event> write_queue_;

 public:
  inline void Enqueue(Event event) {
    std::unique_lock lock{mutex_};
    write_queue_.emplace(std::move(event));
  }

  template <typename T>
  inline void Process(T func) {
    decltype(write_queue_) read_queue;

    {
      std::unique_lock lock{mutex_};
      std::swap(read_queue, write_queue_);
    }

    while (read_queue.size()) {
      std::visit(func, std::move(read_queue.front()));
      read_queue.pop();
    }
  }

  static EventQueue &Get();
};
}  // namespace bitty

#endif /* __BITTY_EVENTS_HH__ */