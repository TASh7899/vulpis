  cmake_minimum_required(VERSION 3.10)

# Setup VCPKG
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

set(VULPIS_MODULE_NETWORK ${VULPIS_MODULE_NETWORK} CACHE BOOL "Enable Network" FORCE)
set(VULPIS_MODULE_DATABASE ${VULPIS_MODULE_DATABASE} CACHE BOOL "Enable Database" FORCE)
set(VULPIS_MODULE_AUDIO ${VULPIS_MODULE_AUDIO} CACHE BOOL "Enable Audio" FORCE)

if(CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_BUILD_TYPE)
    # 1. Enable LTO / IPO (Deletes unused C++ code across all libraries)
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

option(VULPIS_MODULE_NETWORK "Enable HTTP & WebSockets support" OFF)
option(VULPIS_MODULE_DATABASE "Enable SQLite & KVCache support" OFF)
option(VULPIS_MODULE_AUDIO "Enable Audio System" OFF)

find_package(SDL2 CONFIG REQUIRED)
find_package(Lua REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Freetype REQUIRED) 
find_package(yoga CONFIG REQUIRED)
find_package(Threads REQUIRED)
find_package(libzip CONFIG REQUIRED)

# Manual GLAD (internal)
add_library(glad STATIC third_party/glad/src/glad.c)
target_include_directories(glad PUBLIC third_party/glad/include)


# Base Core Sources
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
)

# Base Core Libraries
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
)

# ==============================================================================
# Conditional Modules
# ==============================================================================

# 1. Network Module
if(VULPIS_MODULE_NETWORK)
  find_package(cpr CONFIG REQUIRED)
  find_package(ixwebsocket CONFIG REQUIRED)
  
  list(APPEND VULPIS_SOURCES 
    engine/components/network/http_client.cpp
    engine/components/network/websockets/websockets_client.cpp
  )
  list(APPEND VULPIS_LIBS cpr::cpr ixwebsocket::ixwebsocket)
  add_compile_definitions(VULPIS_MODULE_NETWORK=1)
endif()

# 2. Database Module
if(VULPIS_MODULE_DATABASE)
  find_package(leveldb CONFIG REQUIRED)
  find_package(unofficial-sqlite3 CONFIG REQUIRED)
  
  list(APPEND VULPIS_SOURCES 
    engine/components/database/sqlite_client.cpp
    engine/components/database/kv_cache.cpp
  )
  list(APPEND VULPIS_LIBS leveldb::leveldb unofficial::sqlite3::sqlite3)
  add_compile_definitions(VULPIS_MODULE_DATABASE=1)
endif()

# 3. Audio Module
if(VULPIS_MODULE_AUDIO)
  list(APPEND VULPIS_SOURCES engine/components/audio/audio.cpp)
  add_compile_definitions(VULPIS_MODULE_AUDIO=1)
  
  if (APPLE)
    # macOS requires the CoreAudio and AudioToolbox frameworks for miniaudio
    list(APPEND VULPIS_LIBS "-framework CoreAudio" "-framework AudioToolbox")
  elseif (UNIX AND NOT APPLE)
    # Linux (including Arch) typically requires math and pthread for miniaudio
    list(APPEND VULPIS_LIBS m pthread dl)
  endif()
endif()

# ==============================================================================
# Executable Definition
# ==============================================================================
add_executable(vulpis ${VULPIS_SOURCES})
target_link_libraries(vulpis PRIVATE ${VULPIS_LIBS})

target_include_directories(vulpis PRIVATE
  third_party/glad/include
  ${LUA_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/third_party/stb_image
)

add_executable(asset_baker engine/tools/asset_baker.cpp)

target_link_libraries(asset_baker PRIVATE libzip::zip)

message(STATUS "--- Vulpis Build Configuration ---")
message(STATUS "Network:  ${VULPIS_MODULE_NETWORK}")
message(STATUS "Database: ${VULPIS_MODULE_DATABASE}")
message(STATUS "Audio:    ${VULPIS_MODULE_AUDIO}")
message(STATUS "----------------------------------")

