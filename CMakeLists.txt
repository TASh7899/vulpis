cmake_minimum_required(VERSION 3.10)
project(vulpis LANGUAGES CXX C)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(SDL2 REQUIRED)
find_package(Lua REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED) 

message(STATUS "LUA_INCLUDE_DIR = ${LUA_INCLUDE_DIR}")
message(STATUS "LUA_LIBRARIES   = ${LUA_LIBRARIES}")
message(STATUS "FreeType Include: ${FREETYPE_INCLUDE_DIRS}")

add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)


set(YOGA_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yoga)


add_executable(vulpis
  engine/main.cpp
  engine/components/ui/ui.cpp
  engine/components/color/color.cpp
  engine/components/layout/layout.cpp
  engine/components/layout/yoga.cpp
  engine/components/state/state.cpp
  engine/components/input/input.cpp
  engine/components/vdom/vdom.cpp
  engine/components/renderer/opengl_renderer.cpp
  engine/components/text/font.cpp
)


target_include_directories(vulpis PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${LUA_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party/yoga
  third_party/glad/include
  ${FREETYPE_INCLUDE_DIRS}
)


target_link_libraries(vulpis PRIVATE
  ${SDL2_LIBRARIES}
  ${LUA_LIBRARIES}
  yogacore
  OpenGL::GL
  glad
  ${CMAKE_DL_LIBS}
  ${FREETYPE_LIBRARIES}
)

add_custom_command(TARGET vulpis POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/assets
    ${CMAKE_BINARY_DIR}/src/assets
  COMMENT "Copying assets to build directory.."
)




