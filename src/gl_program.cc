#include "gl_program.hh"

#include <glad/gl.h>

#include <fstream>
#include <glm/mat4x4.hpp>
#include <sstream>

#include "util.hh"

namespace bitty {

GLProgram GLProgram::FromFiles(const std::string &vertex_file,
                               const std::string &fragment_file) {
  std::ifstream vertex_if(vertex_file);
  std::stringstream vertex_ss;
  vertex_ss << vertex_if.rdbuf();

  std::ifstream frag_if(fragment_file);
  std::stringstream frag_ss;
  frag_ss << frag_if.rdbuf();

  return GLProgram(vertex_ss.str(), frag_ss.str());
}

GLProgram::GLProgram(const std::string &vertex, const std::string &fragment) {
  Reset(vertex, fragment);
}

void GLProgram::PrintShaderLog(GLuint id) {
  GLint total_length = 0;
  glGetShaderiv(id, GL_INFO_LOG_LENGTH, &total_length);

  std::string logs;
  logs.resize(total_length);
  glGetShaderInfoLog(id, total_length, NULL, &logs[0]);
  LogError() << logs << std::endl;
}

void GLProgram::PrintProgramLog(GLuint id) {
  GLint total_length = 0;
  glGetProgramiv(id, GL_INFO_LOG_LENGTH, &total_length);

  std::string logs;
  logs.resize(total_length);
  glGetProgramInfoLog(id, total_length, NULL, &logs[0]);
  LogError() << logs << std::endl;
}

void GLProgram::Reset(const std::string &vertex, const std::string &fragment) {
  vertex_str_ = vertex;
  frag_str_ = fragment;

  auto vertex_ptr = vertex_str_.c_str();
  auto frag_ptr = frag_str_.c_str();

  program_ = glCreateProgram();

  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);

  glShaderSource(vertex_shader, 1, &vertex_ptr, nullptr);

  glCompileShader(vertex_shader);

  PrintShaderLog(vertex_shader);

  glAttachShader(program_, vertex_shader);

  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

  glShaderSource(fragment_shader, 1, &frag_ptr, nullptr);

  glCompileShader(fragment_shader);

  PrintShaderLog(fragment_shader);

  glAttachShader(program_, fragment_shader);

  glLinkProgram(program_);
  
  PrintProgramLog(program_);
}

void GLProgram::Use() { glUseProgram(program_); }

template <>
GLint GLProgram::GetUniform(const char *name) {
  auto location = glGetUniformLocation(program_, name);

  GLint val;
  glGetUniformiv(program_, location, &val);

  return val;
}

template <>
float GLProgram::GetUniform(const char *name) {
  auto location = glGetUniformLocation(program_, name);

  float val;
  glGetUniformfv(program_, location, &val);

  return val;
}

template <>
glm::mat4 GLProgram::GetUniform(const char *name) {
  auto location = glGetUniformLocation(program_, name);

  glm::mat4 val;
  glGetUniformfv(program_, location, &val[0][0]);

  return val;
}

template <>
void GLProgram::SetUniform(const char *name, GLint value) {
  auto location = glGetUniformLocation(program_, name);

  glUniform1i(location, value);
}

template <>
void GLProgram::SetUniform(const char *name, float value) {
  auto location = glGetUniformLocation(program_, name);

  glUniform1f(location, value);
}

template <>
void GLProgram::SetUniform(const char *name, glm::mat4 value) {
  auto location = glGetUniformLocation(program_, name);

  glUniformMatrix4fv(location, 1, false, &value[0][0]);
}

}  // namespace bitty