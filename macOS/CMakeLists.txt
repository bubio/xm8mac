#
# eXellent Multi-platform emulator type 8 - 'XM8'
# based on ePC-8801MA
#
# Author (ePC-8801MA) : Takeda.Toshiya
# Author (XM8) : Tanaka.Yasushi
#
# [ CMakeLists for macOS ]
#

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "DO NOT BUILD in-tree.")
endif()

cmake_minimum_required(VERSION 3.10)

# Project Name
project(xm8 VERSION 1.7.1)

# Tool chain
set(CMAKE_CXX_COMPILER "clang++")

# Set macOS version
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.7" CACHE STRING "Minimum OS X deployment version")

# Find SDL2 library and add to linker flags
find_package(SDL2 REQUIRED)

# Add macro definition
add_definitions(-DSDL)
add_definitions(-D_PC8801MA)

# Set compile options
add_compile_options(-Wall
                    -Wno-macro-redefined
                    -Wno-non-c-typedef-for-linkage
                    -Wno-deprecated-declarations
                    -Wno-overloaded-virtual
                    -Wno-c++11-compat-deprecated-writable-strings)

# Include directories
include_directories(
    ../UI
    ../ePC-8801MA
    ../ePC-8801MA/vm
    ../ePC-8801MA/vm/pc8801
    ../ePC-8801MA/vm/fmgen
)


# Set source files
set(SRCS
  ../UI/app.cpp
  ../UI/audio.cpp
  ../UI/converter.cpp
  ../UI/diskmgr.cpp
  ../UI/emu.cpp
  ../UI/emu_sdl.cpp
  ../UI/file.cpp
  ../UI/fileio.cpp
  ../UI/font.cpp
  ../UI/input.cpp
  ../UI/main.cpp
  ../UI/menu.cpp
  ../UI/menuitem.cpp
  ../UI/menulist.cpp
  ../UI/platform.cpp
  ../UI/setting.cpp
  ../UI/softkey.cpp
  ../UI/tapemgr.cpp
  ../UI/video.cpp
  ../ePC-8801MA/common.cpp
  ../ePC-8801MA/config.cpp
  ../ePC-8801MA/fifo.cpp
  ../ePC-8801MA/vm/disk.cpp
  ../ePC-8801MA/vm/event.cpp
  ../ePC-8801MA/vm/i8251.cpp
  ../ePC-8801MA/vm/i8253.cpp
  ../ePC-8801MA/vm/i8255.cpp
  ../ePC-8801MA/vm/disksub.cpp
  ../ePC-8801MA/vm/pcm1bit.cpp
  ../ePC-8801MA/vm/upd765a.cpp
  ../ePC-8801MA/vm/upd1990a.cpp
  ../ePC-8801MA/vm/fmsound.cpp
  ../ePC-8801MA/vm/z80.cpp
  ../ePC-8801MA/vm/fmgen/opna.cpp
  ../ePC-8801MA/vm/fmgen/psg.cpp
  ../ePC-8801MA/vm/fmgen/fmgen.cpp
  ../ePC-8801MA/vm/fmgen/fmtimer.cpp
  ../ePC-8801MA/vm/pc8801/pc88.cpp
  ../ePC-8801MA/vm/pc8801/pc8801.cpp
)

# Set executable
add_executable(${PROJECT_NAME} ${SRCS})

# Set link options
target_link_libraries(${PROJECT_NAME} PRIVATE
                      ${SDL2_LIBRARIES}
                      iconv)