#ifndef __BITTY_GL_PROGRAM_HH__
#define __BITTY_GL_PROGRAM_HH__

#include <glad/gl.h>

#include <string>

namespace bitty {
class GLProgram {
  std::string vertex_str_, frag_str_;
  GLuint program_;

  void PrintShaderLog(GLuint id), PrintProgramLog(GLuint id);

  GLProgram(const GLProgram &) = delete;
  void operator=(const GLProgram &) = delete;
 public:
  static GLProgram FromFiles(const std::string &vertex_file,
                             const std::string &fragment_file);
  GLProgram(const std::string &vertex, const std::string &fragment);
  void Reset(const std::string &vertex, const std::string &fragment);
  void Use();

  template <typename T>
  T GetUniform(const char *name);
  template <typename T>
  void SetUniform(const char *name, T value);

  inline GLuint Id() { return program_; }
};
}  // namespace bitty

#endif /* __BITTY_GL_PROGRAM_HH__ */