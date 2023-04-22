# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

# SDL source files
file(GLOB SDL_SOURCE_FILES
  src/main/cpp/SDL/src/*.c
  src/main/cpp/SDL/src/audio/*.c
  src/main/cpp/SDL/src/audio/android/*.c
  src/main/cpp/SDL/src/audio/dummy/*.c
  src/main/cpp/SDL/src/atomic/*.c
  src/main/cpp/SDL/src/core/android/*.c
  src/main/cpp/SDL/src/cpuinfo/*.c
  src/main/cpp/SDL/src/dynapi/*.c
  src/main/cpp/SDL/src/events/*.c
  src/main/cpp/SDL/src/file/*.c
  src/main/cpp/SDL/src/haptic/*.c
  src/main/cpp/SDL/src/haptic/android/*.c
  src/main/cpp/SDL/src/joystick/*.c
  src/main/cpp/SDL/src/joystick/android/*.c
  src/main/cpp/SDL/src/joystick/steam/*.c
  src/main/cpp/SDL/src/loadso/dlopen/*.c
  src/main/cpp/SDL/src/power/*.c
  src/main/cpp/SDL/src/power/android/*.c
  src/main/cpp/SDL/src/filesystem/android/*.c
  src/main/cpp/SDL/src/render/*.c
  src/main/cpp/SDL/src/render/*/*.c
  src/main/cpp/SDL/src/stdlib/*.c
  src/main/cpp/SDL/src/thread/*.c
  src/main/cpp/SDL/src/thread/pthread/*.c
  src/main/cpp/SDL/src/timer/*.c
  src/main/cpp/SDL/src/timer/unix/*.c
  src/main/cpp/SDL/src/video/*.c
  src/main/cpp/SDL/src/video/android/*.c
  src/main/cpp/SDL/src/video/yuv2rgb/*.c
  src/main/cpp/SDL/src/test/*.c)

# add library for SDL
add_library( SDL2 SHARED ${SDL_SOURCE_FILES} )

# include directories for SDL
target_include_directories( SDL2 PRIVATE src/main/cpp/SDL/include )

# definitions(-D option) for SDL
target_compile_definitions( SDL2 PRIVATE GL_GLEXT_PROTOTYPES )

# link libraries for SDL
find_library( dl-lib dl )
find_library( GLESv1-lib GLESv1_CM )
find_library( GLESv2-lib GLESv2 )
find_library( log-lib log )
find_library( android-lib android )
target_link_libraries( SDL2 ${dl-lib} ${GLESv1-lib} ${GLESv2-lib} ${log-lib} ${android-lib} )

# main source files
file(GLOB MAIN_SOURCE_FILES
  ../../../ePC-8801MA/*.cpp
  ../../../ePC-8801MA/vm/*.cpp
  ../../../ePC-8801MA/vm/fmgen/*.cpp
  ../../../ePC-8801MA/vm/pc8801/*.cpp
  ../../../UI/*.cpp
  src/main/cpp/main/*.c)

# add library for main
add_library( main SHARED ${MAIN_SOURCE_FILES} )

# include directories for main
target_include_directories( main PRIVATE src/main/cpp/main
  src/main/cpp/SDL/include
  ../../../UI
  ../../../ePC-8801MA
  ../../../ePC-8801MA/vm
  ../../../ePC-8801MA/vm/fmgen
  ../../../ePC-8801MA/vm/pc8801 )

# definitions(-D option) for main
target_compile_definitions( main PRIVATE SDL _PC8801MA )

# link libraries for main
target_link_libraries( main SDL2 )
