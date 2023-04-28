# Sets the minimum version of CMake required to build the native library.
cmake_minimum_required(VERSION 3.4.1)

# Get version from environment variable
#set(PROJECT_VERSION 1.7.2)

# Project Name
#project(XM8 VERSION ${PROJECT_VERSION})

set(SDL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/jni/SDL)
set(JNI_PATH ${CMAKE_CURRENT_SOURCE_DIR}/jni/src)
set(XM8_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../Source)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

# SDL source files
file(GLOB SDL_SOURCE_FILES
        ${SDL_PATH}/src/*.c
        ${SDL_PATH}/src/audio/*.c
        ${SDL_PATH}/src/audio/android/*.c
        ${SDL_PATH}/src/audio/dummy/*.c
        ${SDL_PATH}/src/audio/aaudio/*.c
        ${SDL_PATH}/src/audio/openslES/*.c
		${SDL_PATH}/src/atomic/*.c
#	${SDL_PATH}/src/atomic/SDL_atomic.c.arm
#	${SDL_PATH}/src/atomic/SDL_spinlock.c.arm
        ${SDL_PATH}/src/core/android/*.c
        ${SDL_PATH}/src/cpuinfo/*.c
        ${SDL_PATH}/src/dynapi/*.c
        ${SDL_PATH}/src/events/*.c
        ${SDL_PATH}/src/file/*.c
        ${SDL_PATH}/src/haptic/*.c
        ${SDL_PATH}/src/haptic/android/*.c
        ${SDL_PATH}/src/hidapi/*.c
        ${SDL_PATH}/src/hidapi/android/*.cpp
        ${SDL_PATH}/src/joystick/*.c
        ${SDL_PATH}/src/joystick/android/*.c
        ${SDL_PATH}/src/joystick/hidapi/*.c
        ${SDL_PATH}/src/joystick/virtual/*.c
        ${SDL_PATH}/src/loadso/dlopen/*.c
        ${SDL_PATH}/src/locale/*.c
        ${SDL_PATH}/src/locale/android/*.c
        ${SDL_PATH}/src/misc/*.c
        ${SDL_PATH}/src/misc/android/*.c
        ${SDL_PATH}/src/power/*.c
        ${SDL_PATH}/src/power/android/*.c
        ${SDL_PATH}/src/filesystem/android/*.c
        ${SDL_PATH}/src/sensor/*.c
        ${SDL_PATH}/src/sensor/android/*.c
        ${SDL_PATH}/src/render/*.c
        ${SDL_PATH}/src/render/*/*.c
        ${SDL_PATH}/src/stdlib/*.c
        ${SDL_PATH}/src/thread/*.c
        ${SDL_PATH}/src/thread/pthread/*.c
        ${SDL_PATH}/src/timer/*.c
        ${SDL_PATH}/src/timer/unix/*.c
        ${SDL_PATH}/src/video/*.c
        ${SDL_PATH}/src/video/android/*.c
        ${SDL_PATH}/src/video/yuv2rgb/*.c
        ${SDL_PATH}/src/test/*.c
        ${ANDROID_NDK}/sources/android/cpufeatures/cpu-features.c
        )

include(AndroidNdkModules)
android_ndk_import_module_cpufeatures()

# add library for SDL
add_library(SDL2 SHARED ${SDL_SOURCE_FILES})

# include directories for SDL
target_include_directories(SDL2 PRIVATE
        ${SDL_PATH}/include
        )

# definitions(-D option) for SDL
target_compile_definitions(SDL2 PRIVATE GL_GLEXT_PROTOTYPES)
target_compile_options(SDL2 PRIVATE
        -Wall -Wextra
        -Wdocumentation
        -Wdocumentation-unknown-command
        -Wmissing-prototypes
        -Wunreachable-code-break
        -Wunneeded-internal-declaration
        -Wmissing-variable-declarations
        -Wfloat-conversion
        -Wshorten-64-to-32
        -Wunreachable-code-return
        -Wshift-sign-overflow
        -Wstrict-prototypes
        -Wkeyword-macro
        -Wno-unused-parameter
        -Wno-sign-compare
        )

# link libraries for SDL
find_library(dl-lib dl)
find_library(GLESv1-lib GLESv1_CM)
find_library(GLESv2-lib GLESv2)
find_library(OpenSLES-lib OpenSLES)
find_library(log-lib log)
find_library(android-lib android)
target_link_libraries(SDL2 ${dl-lib} ${GLESv1-lib} ${GLESv2-lib} ${OpenSLES-lib} ${log-lib} ${android-lib})

# main source files
file(GLOB MAIN_SOURCE_FILES
        ${XM8_PATH}/ePC-8801MA/*.cpp
        ${XM8_PATH}/ePC-8801MA/vm/*.cpp
        ${XM8_PATH}/ePC-8801MA/vm/fmgen/*.cpp
        ${XM8_PATH}/ePC-8801MA/vm/pc8801/*.cpp
        ${XM8_PATH}/UI/*.cpp
        ${JNI_PATH}/*.c)

# add library for main
add_library(main SHARED ${MAIN_SOURCE_FILES})

# include directories for main
target_include_directories(main PRIVATE
        ${JNI_PATH}
        ${SDL_PATH}/include
        ${XM8_PATH}/UI
        ${XM8_PATH}/ePC-8801MA
        ${XM8_PATH}/ePC-8801MA/vm
        ${XM8_PATH}/ePC-8801MA/vm/fmgen
        ${XM8_PATH}/ePC-8801MA/vm/pc8801)

# definitions(-D option) for main
target_compile_definitions(main PRIVATE SDL _PC8801MA)
target_compile_options(main PRIVATE -Wno-narrowing)

# link libraries for main
target_link_libraries(main SDL2)
