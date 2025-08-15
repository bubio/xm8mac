if(XM8_STATIC_SDL2)
  set(BUILD_SHARED_LIBS OFF)
  set(SDL_SHARED OFF)
  set(SDL_STATIC ON)
  # if(PIE)
  #   set(SDL_STATIC_PIC ON)
  # endif()
else()
  set(BUILD_SHARED_LIBS ON)
  set(SDL_SHARED ON)
  set(SDL_STATIC OFF)
endif()
set(SDL_TEST_ENABLED_BY_DEFAULT OFF)

# Apple環境の場合、システムのiconvを使用する設定
if(APPLE)
  set(SDL_SYSTEM_ICONV ON CACHE BOOL "Use iconv() from system-installed libraries")
  set(SDL_LIBICONV OFF CACHE BOOL "Prefer iconv() from libiconv, if available, over libc version")
endif()

include(functions/FetchContent_ExcludeFromAll_backport)
include(FetchContent)
FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.32.8/SDL2-2.32.8.tar.gz
    URL_HASH SHA256=0ca83e9c9b31e18288c7ec811108e58bac1f1bb5ec6577ad386830eac51c787e
)
FetchContent_MakeAvailable_ExcludeFromAll(SDL2)