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

set(MSDFGEN_BUILD_MSDFGEN_STANDALONE OFF CACHE BOOL "Disable msdfgen standalone executable" FORCE)
set(MSDFGEN_USE_FREETYPE ON CACHE BOOL "Enable FreeType support in msdfgen" FORCE)
set(MSDFGEN_USE_TINYXML2 ON CACHE BOOL "Enable TinyXML2 in msdfgen" FORCE)
set(MSDFGEN_USE_SKIA OFF CACHE BOOL "Disable Skia in msdfgen" FORCE)

add_subdirectory(third_party/msdfgen)


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
  engine/configLogic/images/texture_registry.cpp
)

target_link_libraries(vulpis PRIVATE
  $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  
  yoga::yogacore
  
  ${LUA_LIBRARIES}
  OpenGL::GL
  glad
  Freetype::Freetype
  msdfgen::msdfgen
  msdfgen::msdfgen-ext

  ${CMAKE_DL_LIBS}
)


find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(vulpis PUBLIC OpenMP::OpenMP_CXX)
    target_compile_options(vulpis PRIVATE ${OpenMP_CXX_FLAGS})
endif()

target_include_directories(vulpis PRIVATE 
  third_party/glad/include
  ${LUA_INCLUDE_DIR}
)

add_executable(asset_baker engine/tools/asset_baker.cpp)

set(ASSETS_SOURCE "${CMAKE_SOURCE_DIR}/assets")
set(ASSETS_DEST "${CMAKE_BINARY_DIR}/assets")

set(BAKED_SOURCE "${CMAKE_SOURCE_DIR}/baked")
set(BAKED_DEST "${CMAKE_BINARY_DIR}/baked")

set(FONTS_SOURCE "${CMAKE_SOURCE_DIR}/third_party/NotoSans")
set(FONTS_DEST "${ASSETS_DEST}/builtin/NotoSans")

add_custom_target(BakeAssets ALL
  COMMAND $<TARGET_FILE:asset_baker> "${ASSETS_SOURCE}" "${BAKED_SOURCE}"
  DEPENDS asset_baker
  COMMENT "Baking textures into .vtex binaries..."
)

if (WIN32)
  add_custom_target(SyncAssets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BAKED_SOURCE}" # Safeguard if folder is deleted
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${ASSETS_SOURCE}" "${ASSETS_DEST}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${BAKED_SOURCE}" "${BAKED_DEST}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${FONTS_SOURCE}" "${FONTS_DEST}"
    COMMENT "Syncing assets, baked cache, and fonts..."
  )
else()
  add_custom_target(SyncAssets ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BAKED_SOURCE}" # Safeguard if folder is deleted
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${ASSETS_SOURCE}" "${ASSETS_DEST}"
    COMMAND ${CMAKE_COMMAND} -E create_symlink "${BAKED_SOURCE}" "${BAKED_DEST}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${FONTS_DEST}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${FONTS_SOURCE}" "${FONTS_DEST}"
    COMMENT "Ensuring assets symlink and fonts exist..."
  )
endif()

add_dependencies(SyncAssets BakeAssets)
add_dependencies(vulpis SyncAssets)
