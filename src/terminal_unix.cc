#include <GLFW/glfw3.h>
#include <asm-generic/ioctls.h>
#include <fcntl.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "cell_buffer.hh"
#include "events.hh"
#include "terminal.hh"
#include "util.hh"

namespace bitty {
Terminal::Terminal(const std::string &shell_path, u32 init_w, u32 init_h) {
  char *slave_device;

  pt_master_no_ = posix_openpt(O_RDWR);

  if (pt_master_no_ == -1 || grantpt(pt_master_no_) == -1 ||
      unlockpt(pt_master_no_) == -1 || !(slave_device = ptsname(pt_master_no_)))
    throw std::runtime_error("Failed to open master pty");

  MakeBuffer(init_w, init_h);

  event_fd_ = eventfd(0, 0);
  if (event_fd_ == -1) throw std::runtime_error("Failed to create eventfd");

  if (int new_id = fork(); new_id == 0) {
    int slave_fd = open(slave_device, O_RDWR);

    char *shell_path_c = strdup(shell_path.c_str());

    char *const argv[] = {shell_path_c, nullptr};

    dup2(slave_fd, STDIN_FILENO);
    dup2(slave_fd, STDOUT_FILENO);
    dup2(slave_fd, STDERR_FILENO);

    close(pt_master_no_);

    if (setsid() < 0) throw std::runtime_error("Failed to execute setsid?");

    if (ioctl(0, TIOCSCTTY, 1) < 0)
      throw std::runtime_error("Failed to execute ioctl?");

    setenv("TERM", "kitty", 1);

    if (execvp(shell_path.c_str(), argv) < 0)
      throw std::runtime_error("Failed to execute execvp?");
  }

  thread_ = std::thread([&] {
    for (;;) {
      struct pollfd fds[2] = {};
      fds[0].fd = pt_master_no_;
      fds[1].fd = event_fd_;
      fds[0].events = POLLIN;
      fds[1].events = POLLIN;

      if (poll(fds, 2, -1) == -1) {
        LogError() << "poll(...) call failed?" << std::endl;
        break;
      }

      if (fds[1].revents & POLLIN) break;

      if (fds[0].revents & POLLIN) {
        std::unique_ptr<std::byte[]> bytes{new std::byte[kReadChunkSize]};
        ssize_t bytes_read = read(pt_master_no_, bytes.get(), kReadChunkSize);

        if (bytes_read > 0) {
          EventQueue::Get().Enqueue(
              EventDataFromTty{.terminal_id = Id(),
                               .bytes = std::move(bytes),
                               .byte_count = (size_t)bytes_read});
          glfwPostEmptyEvent();
        } else
          LogError() << "read(...) call failed?" << std::endl;
      }
    }
  });
}

void Terminal::SetWindowSize(uint32_t width, uint32_t height) {
  struct winsize w = {};
  w.ws_col = width;
  w.ws_row = height;
  ioctl(pt_master_no_, TIOCSWINSZ, &w);

  auto [delta_w, delta_vh] = buf_->Resize(width, height);
  cursor_y_ = std::min(cursor_y_, int(height - 1));
  cursor_x_ = std::min(cursor_x_, int(width - 1));
  scroll_area_.right += delta_w;
  scroll_area_.bottom += delta_vh;

  if (alternate_buf_) alternate_buf_->Resize(width, height);
}

void Terminal::WriteToPty(std::vector<char> &&bytes) {
  write(pt_master_no_, bytes.data(), bytes.size());
}

Terminal::~Terminal() {
  uint64_t value = 1;
  write(event_fd_, &value, sizeof(uint64_t));
  thread_.join();
  close(event_fd_);
}

}  // namespace bitty
