# Options that control whether to use system dependencies or build them from source,
# and whether to link them statically.
include(functions/dependency_options)

dependency_options("SDL2" XM8_SYSTEM_SDL2 ON XM8_STATIC_SDL2)

if(XM8_SYSTEM_SDL2)
  find_package(SDL2 REQUIRED)
  if(TARGET SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
      set(SDL2_MAIN SDL2::SDL2main)
    endif()
  elseif(TARGET SDL2::SDL2-static)
    # On some distros, such as vitasdk, only the SDL2::SDL2-static target is available.
    # Alias to SDL2::SDL2 because some finder scripts may refer to SDL2::SDL2.
    add_library(SDL2::SDL2 ALIAS SDL2::SDL2-static)
    if(TARGET SDL2::SDL2main)
      set(SDL2_MAIN SDL2::SDL2main)
    endif()
  else()
    # Assume an older Debian derivate that comes with an sdl2-config.cmake
    # that only defines `SDL2_LIBRARIES` (as -lSDL2) and `SDL2_INCLUDE_DIRS`.
    add_library(SDL2_lib INTERFACE)
    target_link_libraries(SDL2_lib INTERFACE ${SDL2_LIBRARIES})
    target_include_directories(SDL2_lib INTERFACE ${SDL2_INCLUDE_DIRS})
    # Can't define an INTERFACE target with ::, so alias instead
    add_library(SDL2::SDL2 ALIAS SDL2_lib)
  endif()
else()
  add_subdirectory(Builder/External/SDL2)
  if(TARGET SDL2::SDL2main)
    set(SDL2_MAIN SDL2::SDL2main)
  endif()
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Builder/External/SDL2/CMake")
endif()

add_library(XM8::SDL INTERFACE IMPORTED GLOBAL)
if(TARGET SDL2::SDL2)
  target_link_libraries(XM8::SDL INTERFACE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
  target_link_libraries(XM8::SDL INTERFACE SDL2::SDL2-static)
endif()
if(NOT UWP_LIB)
  target_link_libraries(XM8::SDL INTERFACE ${SDL2_MAIN})
endif()