cmake_minimum_required(VERSION 3.10)

project(vulpis LANGUAGES CXX C)

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO Enabled: The final binary will be stripped of dead code.")
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Os)
    elseif(MSVC)
        add_compile_options(/O1) 
    endif()
endif()

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
find_package(Threads REQUIRED)
find_package(libzip CONFIG REQUIRED)

find_package(cpr CONFIG REQUIRED)
find_package(ixwebsocket CONFIG REQUIRED)
find_package(leveldb CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)

set(VULPIS_SOURCES
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
  engine/components/system/secure_storage.cpp
  engine/components/system/system_bindings.cpp
  engine/configLogic/font/font_registry.cpp
  engine/configLogic/engineConf/engine_config.cpp
  engine/configLogic/images/texture_registry.cpp
  engine/tools/stats_logger/stats_logger.cpp
  engine/components/network/http_client.cpp
  engine/components/network/websockets/websockets_client.cpp
  engine/components/database/sqlite_client.cpp
  engine/components/database/kv_cache.cpp
  engine/components/audio/audio.cpp
)

set(VULPIS_LIBS
  $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
  $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
  yoga::yogacore
  ${LUA_LIBRARIES}
  OpenGL::GL
  glad
  Freetype::Freetype
  libzip::zip
  ${CMAKE_DL_LIBS}
  cpr::cpr 
  ixwebsocket::ixwebsocket
  leveldb::leveldb 
  unofficial::sqlite3::sqlite3
)

# Clean, single OS block (without bcrypt)
if (APPLE)
  list(APPEND VULPIS_LIBS "-framework CoreAudio" "-framework AudioToolbox")
elseif (UNIX AND NOT APPLE)
  list(APPEND VULPIS_LIBS m pthread dl)
endif()

add_executable(vulpis ${VULPIS_SOURCES})
target_link_libraries(vulpis PRIVATE ${VULPIS_LIBS})

if (WIN32)
    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lbcrypt")
endif()

target_include_directories(vulpis PRIVATE
  third_party/glad/include
  ${LUA_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party/stb_image
)

add_executable(asset_baker engine/tools/asset_baker.cpp)
target_link_libraries(asset_baker PRIVATE libzip::zip)
