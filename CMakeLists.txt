cmake_minimum_required(VERSION 3.10)

set(VCPKG_ROOT "${CMAKE_SOURCE_DIR}/third_party/vcpkg")

if(EXISTS "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
else()
  if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
  else()
    message(WARNING "Vcpkg not found in third_party/vcpkg or ENV{VCPKG_ROOT}. Dependency installation may fail.")
  endif()
endif()

project(vulpis LANGUAGES CXX C)

set(OpenGL_GL_PREFERENCE GLVND)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED) 
find_package(yoga CONFIG REQUIRED)

# 3. Manual GLAD (internal)
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

# 4. Executable Definition
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
  engine/components/system/pathUtils.cpp
  engine/configLogic/font/font_registry.cpp
  engine/configLogic/engineConf/engine_config.cpp
)

target_link_libraries(vulpis PRIVATE
  $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  
  yoga::yogacore
  
  ${LUA_LIBRARIES}
  OpenGL::GL
  glad
  Freetype::Freetype
  ${CMAKE_DL_LIBS}
)

target_include_directories(vulpis PRIVATE 
  third_party/glad/include
  ${LUA_INCLUDE_DIR}
)

set(ASSETS_SOURCE "${CMAKE_SOURCE_DIR}/assets")
set(ASSETS_DEST "${CMAKE_BINARY_DIR}/assets")
set(FONTS_SOURCE "${CMAKE_SOURCE_DIR}/third_party/NotoSans")
set(FONTS_DEST "${CMAKE_BINARY_DIR}/builtin/NotoSans")

if (WIN32)
  add_custom_target(SyncAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${ASSETS_SOURCE} ${ASSETS_DEST}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${FONTS_SOURCE} ${FONTS_DEST} # Copy Fonts
    COMMENT "Syncing assets and fonts..."
  )
else()
  add_custom_target(SyncAssets ALL
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${ASSETS_SOURCE} ${ASSETS_DEST}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${FONTS_DEST}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${FONTS_SOURCE} ${FONTS_DEST}
    COMMENT "Ensuring assets symlink and fonts exist..."
  )
endif()
add_dependencies(vulpis SyncAssets)
