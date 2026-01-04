cmake_minimum_required(VERSION 3.10)
project(vulpis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(SDL2 REQUIRED)
find_package(Lua REQUIRED)

message(STATUS "LUA_INCLUDE_DIR = ${LUA_INCLUDE_DIR}")
message(STATUS "LUA_LIBRARIES   = ${LUA_LIBRARIES}")


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
  engine/components/core/window.cpp
)


target_include_directories(vulpis PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${LUA_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party/yoga
)


target_link_libraries(vulpis PRIVATE
  ${SDL2_LIBRARIES}
  ${LUA_LIBRARIES}
  yogacore
)

