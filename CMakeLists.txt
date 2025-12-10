cmake_minimum_required(VERSION 3.10)
project(vulpis)

set(CMAKE_CXX_STANDARD 17)

find_package(SDL2 REQUIRED)
find_package(Lua REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
include_directories(${Lua_INCLUDE_DIRS})

add_executable(vulpis
  src/main.cpp
  src/ui.cpp
)

target_link_libraries(vulpis
  ${SDL2_LIBRARIES}
  ${LUA_LIBRARIES}
)
