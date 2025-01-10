#include "util.hh"

#include <iostream>

#include "charset.hh"

namespace bitty {
bool CheckGLErrors() {
  if (GLenum err = glGetError()) {
    LogError() << "GL error: " << err << std::endl;

    return false;
  }

  return true;
}

void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id,
                                GLenum severity, GLsizei length,
                                const GLchar* message, const void* userParam) {
  (void)source;
  (void)id;
  (void)severity;
  (void)length;
  (void)userParam;

  fprintf(stderr,
          "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
          (type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : ""), type, severity,
          message);
}

void EnableGLDebugOutput() {
  // During init, enable debug output
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(MessageCallback, 0);
}
}  // namespace bitty