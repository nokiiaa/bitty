#include <freetype2/ft2build.h>
#include <glad/gl.h>

#include <glm/ext/matrix_transform.hpp>

#include "cell_buffer.hh"
#include "events.hh"
#include "font_renderer.hh"
#include "term_renderer.hh"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

#include "terminal.hh"
#include "util.hh"

static void error_callback(int error, const char *description) {
  fprintf(stderr, "Error %i: %s\n", error, description);
}

namespace bitty::callbacks {
static void MouseMoved(GLFWwindow *window, double x_pos, double y_pos) {
  (void)window;
  EventQueue::Get().Enqueue(EventMousePos{x_pos, y_pos});
}

static void ScrollReceived(GLFWwindow *window, double x_offset,
                           double y_offset) {
  (void)window;
  EventQueue::Get().Enqueue(EventMouseScroll{x_offset, y_offset});
}

static void MouseBtnReceived(GLFWwindow *window, int button, int action,
                             int mods) {
  (void)window;
  EventQueue::Get().Enqueue(EventMouseButton{button, action, mods});
}

static void KeyReceived(GLFWwindow *window, int key, int scancode, int action,
                        int mods) {
  (void)window;
  EventQueue::Get().Enqueue(EventKeyInput{key, scancode, action, mods});
}

static void CharReceived(GLFWwindow *window, uint32_t chr) {
  (void)window;
  EventQueue::Get().Enqueue(EventCharInput{chr});
}

static void WindowSizeChanged(GLFWwindow *window, int new_width,
                              int new_height) {
  (void)window;
  EventQueue::Get().Enqueue(EventWindowResized{new_width, new_height});
}

static void WindowPosChanged(GLFWwindow *, int, int) {}

}  // namespace bitty::callbacks

using namespace bitty;

#ifdef __linux__
// clang-format off
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Intrinsic.h>
#include <X11/Core.h>
#include <X11/Xatom.h>
// clang-format on

bool BlurWindow(GLFWwindow *window, int blur_radius) {
  Display *display = glfwGetX11Display();
  Window window_handle = glfwGetX11Window(window);

  auto _KDE_NET_WM_BLUR_BEHIND_REGION =
      XInternAtom(display, "_KDE_NET_WM_BLUR_BEHIND_REGION", True);

  if (_KDE_NET_WM_BLUR_BEHIND_REGION != None) {
    uint32_t data = 0;
    if (blur_radius > 0)
      XChangeProperty(display, window_handle, _KDE_NET_WM_BLUR_BEHIND_REGION,
                      XA_CARDINAL, 32, PropModeReplace, (unsigned char *)&data,
                      1);
    else
      XDeleteProperty(display, window_handle, _KDE_NET_WM_BLUR_BEHIND_REGION);
    return true;
  }

  return false;
}
#else
bool BlurWindow(GLFWwindow *window, int blur_radius) { return false; }
#endif

int main() {
  GLFWwindow *window;

  glfwSetErrorCallback(error_callback);

  if (!glfwInit()) exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, 1);

  const GLFWvidmode *mode = glfwGetVideoMode(glfwGetPrimaryMonitor());

  auto [scr_w, scr_h] = std::tuple{mode->width, mode->height};

  auto [win_w, win_h] = std::tuple{2 * scr_w / 3, 2 * scr_h / 3};

  window = glfwCreateWindow(win_w, win_h, "bitty", NULL, NULL);

  glfwSetWindowPos(window, scr_w / 2 - win_w / 2, scr_h / 2 - win_h / 2);

  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetCursorPosCallback(window, callbacks::MouseMoved);
  glfwSetMouseButtonCallback(window, callbacks::MouseBtnReceived);
  glfwSetScrollCallback(window, callbacks::ScrollReceived);
  glfwSetKeyCallback(window, callbacks::KeyReceived);
  glfwSetCharCallback(window, callbacks::CharReceived);
  glfwSetWindowSizeCallback(window, callbacks::WindowSizeChanged);
  glfwSetWindowPosCallback(window, callbacks::WindowPosChanged);
  
  BlurWindow(window, 2);

  glfwMakeContextCurrent(window);
  gladLoadGL(glfwGetProcAddress);
  glfwSwapInterval(0);

  EnableGLDebugOutput();

  TermRenderer renderer;
  bool needs_redraw = true;

  int width, height;

  glfwGetFramebufferSize(window, &width, &height);

  int pty_id =
      Terminal::Create(Config::Get().ShellPath(), width / GlobalCellWidthPx(),
                       height / GlobalCellHeightPx());

  std::shared_ptr terminal{Terminal::Get(pty_id).value_or(nullptr)};

  bool set_win_size = true;

  while (!glfwWindowShouldClose(window)) {
    if (needs_redraw) {
      glfwGetFramebufferSize(window, &width, &height);

      glViewport(0, 0, width, height);
      glClearColor(0, 0, 0, Config::Get().Opacity());
      glClear(GL_COLOR_BUFFER_BIT);

      if (auto buf = terminal->CurrentBuffer()) {
        auto [w, h] = std::tuple{buf->ScreenWidth(), buf->ScreenHeight()};
        if (set_win_size) {
          glfwSetWindowSize(window, w, h);
          set_win_size = false;
        }
        renderer.Render(*terminal, width, height);
      }

      glfwSwapBuffers(window);

      needs_redraw = false;
    }

    glfwWaitEvents();

    EventQueue::Get().Process(Overloaded{
        [&](EventMouseScroll scroll) mutable {
          terminal->HandleMouseScroll(scroll);

          needs_redraw = true;
        },
        [&](EventMousePos pos) mutable { terminal->HandleMousePos(pos); },
        [&](EventMouseButton mouse) mutable {
          terminal->HandleMouseButton(mouse);
        },
        [&](EventKeyInput keystroke) mutable {
          // LogInfo() << keystroke.action << ' ' << keystroke.key << ' '
          //           << keystroke.mods << ' ' << keystroke.scancode << '\n';

          if (keystroke.action != GLFW_RELEASE) {
            switch (keystroke.key) {
              case GLFW_KEY_ENTER:
                terminal->WriteToPty({'\r'});
                if (terminal->IsLNMSet()) terminal->WriteToPty({'\n'});
                break;

              case GLFW_KEY_BACKSPACE:
                terminal->WriteToPty({'\b'});
                break;

              case GLFW_KEY_UP:
                terminal->WriteToPty({'\e', '[', 'A'});
                break;

              case GLFW_KEY_TAB:
                terminal->WriteToPty({'\t'});
                break;

              case GLFW_KEY_ESCAPE:
                terminal->WriteToPty({'\e'});
                break;

              case GLFW_KEY_DOWN:
                terminal->WriteToPty({'\e', '[', 'B'});
                break;

              case GLFW_KEY_RIGHT:
                terminal->WriteToPty({'\e', '[', 'C'});
                break;

              case GLFW_KEY_LEFT:
                terminal->WriteToPty({'\e', '[', 'D'});
                break;

              // These keys seem to be consecutive as of this glfw version
              case GLFW_KEY_A:
              case GLFW_KEY_B:
              case GLFW_KEY_C:
              case GLFW_KEY_D:
              case GLFW_KEY_E:
              case GLFW_KEY_F:
              case GLFW_KEY_G:
              case GLFW_KEY_H:
              case GLFW_KEY_I:
              case GLFW_KEY_J:
              case GLFW_KEY_K:
              case GLFW_KEY_L:
              case GLFW_KEY_M:
              case GLFW_KEY_N:
              case GLFW_KEY_O:
              case GLFW_KEY_P:
              case GLFW_KEY_Q:
              case GLFW_KEY_R:
              case GLFW_KEY_S:
              case GLFW_KEY_T:
              case GLFW_KEY_U:
              case GLFW_KEY_V:
              case GLFW_KEY_W:
              case GLFW_KEY_X:
              case GLFW_KEY_Y:
              case GLFW_KEY_Z:
                if (keystroke.mods & GLFW_MOD_CONTROL)
                  terminal->WriteToPty({char(keystroke.key - GLFW_KEY_A + 1)});
                break;
            }
          }

          needs_redraw = true;
        },

        [&](EventCharInput chr) mutable {
          char32_t codepoint = chr.code;

          std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> convert;
          auto byte_str = convert.to_bytes(&codepoint, &codepoint + 1);

          if (terminal->IsUserScrolledUp()) terminal->TryResetUserScroll();

          terminal->WriteToPty(std::vector(byte_str.begin(), byte_str.end()));
        },

        [&](EventDataFromTty data) mutable {
          for (size_t i = 0; i < data.byte_count; i++)
            terminal->InterpretPtyInput((char)data.bytes[i]);
          needs_redraw = true;
        },

        [&](EventWindowResized resized) mutable {
          (void)resized;
          u32 cw = GlobalCellWidthPx();
          u32 ch = GlobalCellHeightPx();
          u32 nw = (u32)resized.new_width / cw;
          u32 nh = (u32)resized.new_height / ch;
          terminal->SetWindowSize(nw, nh);
          glfwSetWindowSize(window, nw * cw, nh * ch);
          needs_redraw = true;
        }});
  }

  glfwDestroyWindow(window);

  glfwTerminate();
  return 0;
}
