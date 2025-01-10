#include "events.hh"

namespace bitty {
EventQueue &EventQueue::Get() {
  static EventQueue queue;
  return queue;
}
}  // namespace bitty