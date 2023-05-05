# Options that control whether to use system dependencies or build them from source,
# and whether to link them statically.
include(functions/dependency_options)

dependency_options("SDL2" XM8_SYSTEM_SDL2 ON XM8_STATIC_SDL2)

add_subdirectory(Builder/External/SDL2)
if(TARGET SDL2::SDL2main)
  set(SDL2_MAIN SDL2::SDL2main)
endif()
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Builder/External/SDL2/CMake")

add_library(XM8::SDL INTERFACE IMPORTED GLOBAL)
if(TARGET SDL2::SDL2)
  target_link_libraries(XM8::SDL INTERFACE SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
  target_link_libraries(XM8::SDL INTERFACE SDL2::SDL2-static)
endif()
if(NOT UWP_LIB)
  target_link_libraries(XM8::SDL INTERFACE ${SDL2_MAIN})
endif()