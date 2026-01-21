cmake_minimum_required(VERSION 3.10)
project(vulpis LANGUAGES CXX C ASM)
cmake_policy(SET CMP0072 NEW)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUNDLE_FONTS "Embed Iosevka fonts into the binary" ON)

if(NOT BUNDLE_FONTS)
    # Defines the macro globally for all targets
    add_definitions(-DDISABLE_BUNDLE_FONT)
endif()

find_package(SDL2 CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(thorvg REQUIRED IMPORTED_TARGET thorvg)
find_package(Lua REQUIRED)
find_package(Freetype REQUIRED)
find_package(yoga CONFIG REQUIRED)

find_package(OpenGL REQUIRED)

#          ┏╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┓
#          ╏                 AUTO GENERATE FONT FILES                 ╏
#          ┗╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍╍┛


set(FONT_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/fonts/iosevka")
set(FONT_OUTPUT_DIR "${CMAKE_BINARY_DIR}/generated/components/text/iosevka")
file(MAKE_DIRECTORY ${FONT_OUTPUT_DIR})

set(BUNDLED_FONTS
    sgr_iosevkaslab_thin
    sgr_iosevkaslab_extraLight
    sgr_iosevkaslab_light
    sgr_iosevkaslab_regular
    sgr_iosevkaslab_medium
    sgr_iosevkaslab_semiBold
    sgr_iosevkaslab_bold
    sgr_iosevkaslab_extraBold
    sgr_iosevkaslab_heavy
)

set(GENERATED_FONT_SOURCES "")

set(REGISTRY_HEADER "${FONT_OUTPUT_DIR}/bundled_fonts_decl.h")
file(WRITE ${REGISTRY_HEADER} "#pragma once\n\n")

foreach(FONT_NAME ${BUNDLED_FONTS})
    set(INPUT_FILE "${FONT_SOURCE_DIR}/${FONT_NAME}.ttc")

    set(ASM_FILE "${FONT_OUTPUT_DIR}/${FONT_NAME}.S")
    set(VAR_NAME "${FONT_NAME}_ttc")

    file(APPEND ${REGISTRY_HEADER} "extern unsigned char ${VAR_NAME}[];\n")
    file(APPEND ${REGISTRY_HEADER} "extern unsigned int ${VAR_NAME}_len;\n")

    file(WRITE ${ASM_FILE} "
    .section .rodata
    .global ${VAR_NAME}
    .global ${VAR_NAME}_len
    .align 16

    ${VAR_NAME}:
    .incbin \"${INPUT_FILE}\"
    ${VAR_NAME}_end:

    .align 4
    ${VAR_NAME}_len:
    .int ${VAR_NAME}_end - ${VAR_NAME}
    ")

    list(APPEND GENERATED_FONT_SOURCES ${ASM_FILE})
endforeach()

  add_library(BundledFonts STATIC ${GENERATED_FONT_SOURCES})

  target_include_directories(BundledFonts PUBLIC 
    ${CMAKE_BINARY_DIR}/generated
  )


  add_library(glad STATIC third_party/glad/src/glad.c)
  target_include_directories(glad PUBLIC third_party/glad/include)

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
    engine/components/text/tvg_ui.cpp
  )


  target_include_directories(vulpis PRIVATE
    third_party/glad/include
    ${LUA_INCLUDE_DIR}
  )


  target_link_libraries(vulpis PRIVATE
    BundledFonts
    SDL2::SDL2
    PkgConfig::thorvg
    Freetype::Freetype
    ${LUA_LIBRARIES}
    yoga::yogacore
    OpenGL::GL
    glad
  )






