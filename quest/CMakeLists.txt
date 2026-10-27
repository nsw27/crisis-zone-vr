cmake_minimum_required(VERSION 3.22)
project(crisiszonevr C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(UP "${ROOT}/upstream")
set(SDL_ROOT "${ROOT}/.tools/sdl/SDL2-2.30.11")
set(XR "${ROOT}/.tools/openxr/prefab/modules")
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI ON CACHE BOOL "" FORCE)
add_subdirectory("${SDL_ROOT}" "${CMAKE_BINARY_DIR}/sdl")
# Upstream uses <SDL2/SDL.h>; SDL's source build installs headers under include/.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/compat/SDL2")
file(GLOB SDL_HEADERS "${SDL_ROOT}/include/*.h")
foreach(header ${SDL_HEADERS})
  get_filename_component(name "${header}" NAME)
  configure_file("${header}" "${CMAKE_BINARY_DIR}/compat/SDL2/${name}" COPYONLY)
endforeach()
include(tc2_sources.cmake)

# Expected NDK revision. CI and Stage B must both use this exact release.
set(TC2_NDK_REVISION "29.0.14206865")

# TC2_USE_PRECOMPILED_CORE=<path to libcrisiszone_core.a>
# When set, the expensive lifted sources are NOT compiled. The archive is imported.
set(TC2_USE_PRECOMPILED_CORE "" CACHE FILEPATH "Prebuilt libcrisiszone_core.a from CI")

if(TC2_USE_PRECOMPILED_CORE)
  if(NOT EXISTS "${TC2_USE_PRECOMPILED_CORE}")
    message(FATAL_ERROR "TC2_USE_PRECOMPILED_CORE not found: ${TC2_USE_PRECOMPILED_CORE}")
  endif()
  add_library(crisiszone_core STATIC IMPORTED)
  set_target_properties(crisiszone_core PROPERTIES IMPORTED_LOCATION "${TC2_USE_PRECOMPILED_CORE}")
else()
  # ROM-independent static core. CI builds only this and uploads it.
  add_library(crisiszone_core STATIC ${TC2_CORE_SOURCES})
  set_target_properties(crisiszone_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
  target_compile_definitions(crisiszone_core PRIVATE TCVR XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_OPENGL_ES)
  target_compile_options(crisiszone_core PRIVATE -O2 -fno-strict-aliasing -fwrapv -Wall -Wno-unused-function -Wno-unused-variable -Wno-misleading-indentation)
  target_include_directories(crisiszone_core PRIVATE . include "${CMAKE_BINARY_DIR}/compat" "${TC}/gen" "${TC}/include" "${TC}/src" "${TC}/src/eng" "${XR}/headers/include")
endif()

# Production shared library. Requires real csz_sub.c / csz_io.c (from the ROM).
# Not built by the CI core-only job.
if(NOT TC2_CORE_ONLY)
  add_library(main SHARED quest_main.c ${TC2_H8_SOURCES})
  target_compile_definitions(main PRIVATE TCVR XR_USE_PLATFORM_ANDROID XR_USE_GRAPHICS_API_OPENGL_ES)
  target_compile_options(main PRIVATE -O2 -fno-strict-aliasing -fwrapv -Wall -Wno-unused-function -Wno-unused-variable -Wno-misleading-indentation)
  target_include_directories(main PRIVATE . include "${CMAKE_BINARY_DIR}/compat" "${TC}/gen" "${TC}/include" "${TC}/src" "${TC}/src/eng" "${XR}/headers/include")
  add_library(openxr_loader SHARED IMPORTED)
  set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION "${XR}/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so")
  target_link_libraries(main crisiszone_core SDL2 openxr_loader GLESv3 EGL android log z m)
  target_link_options(main PRIVATE "-Wl,-z,max-page-size=16384" "-Wl,--no-undefined")
endif()
