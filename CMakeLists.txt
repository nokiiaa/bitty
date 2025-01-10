cmake_minimum_required(VERSION 3.15)

project(bitty C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)

find_package(X11 REQUIRED)
find_package(glfw3 REQUIRED)
find_package(Freetype REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(boost_dynamic_bitset REQUIRED CONFIG)
find_package(boost_container_hash REQUIRED CONFIG)
find_package(glm REQUIRED CONFIG)

add_compile_options(-Wall -Wextra)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-fsanitize=undefined -fsanitize=address)
  add_link_options(-fsanitize=undefined -fsanitize=address)
else()
  add_compile_options(-O3 -g)
endif()

add_executable(${PROJECT_NAME}
  src/main.cc
  src/config.cc
  src/font_renderer.cc
  src/charset.cc
  src/gl_program.cc
  src/util.cc
  src/term_renderer.cc
  src/events.cc
  src/terminal_unix.cc
  src/terminal.cc
  src/escape_parser.cc
  src/cell_buffer.cc
  external/glad/src/gl.c
)

find_path(LINMATH_INCLUDE_DIRS "linmath.h")

set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_STANDARD 26)

target_include_directories(${PROJECT_NAME} PRIVATE external/glad/include include)
target_include_directories(${PROJECT_NAME} PRIVATE ${LINMATH_INCLUDE_DIRS}/linmath.h)

target_compile_definitions(bitty
    PRIVATE
        $<$<CONFIG:Debug>:TERM_DEBUG>
)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE Freetype::Freetype)
target_link_libraries(${PROJECT_NAME} PRIVATE Fontconfig::Fontconfig)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::dynamic_bitset)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::container_hash)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE X11)
