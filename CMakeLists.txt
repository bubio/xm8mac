#
# eXellent Multi-platform emulator type 8 - 'XM8'
# based on ePC-8801MA
#
# Author (ePC-8801MA) : Takeda.Toshiya
# Author (XM8) : Tanaka.Yasushi
#
# [ CMakeLists for macOS/Linux ]
#

cmake_minimum_required(VERSION 3.15)

foreach(_policy CMP0111 CMP0126 CMP0135)
  if(POLICY ${_policy})
    cmake_policy(SET ${_policy} NEW)
    set(CMAKE_POLICY_DEFAULT_${_policy} NEW)
  endif()
endforeach()

if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR "DO NOT BUILD in-tree.")
endif()

# Get version from environment variable
set(PROJECT_VERSION 1.7.3)

# Project Name
project(XM8 VERSION ${PROJECT_VERSION})

# Find SDL2 library and add to linker flags
find_package(SDL2 REQUIRED)

# Add macro definition
add_definitions(-DSDL)
add_definitions(-D_PC8801MA)

# Set compile options
add_compile_options(-Wall)
if(APPLE)
  add_compile_options(
    -Wno-macro-redefined
    -Wno-non-c-typedef-for-linkage
    -Wno-deprecated-declarations
    -Wno-overloaded-virtual
    -Wno-c++11-compat-deprecated-writable-strings)
else()
  add_compile_options(-Wno-narrowing)
endif()

# Include directories
include_directories(
  Source/UI
  Source/ePC-8801MA
  Source/ePC-8801MA/vm
  Source/ePC-8801MA/vm/pc8801
  Source/ePC-8801MA/vm/fmgen
)

# Set source files
set(SRCS
  Source/UI/app.cpp
  Source/UI/audio.cpp
  Source/UI/converter.cpp
  Source/UI/diskmgr.cpp
  Source/UI/emu.cpp
  Source/UI/emu_sdl.cpp
  Source/UI/file.cpp
  Source/UI/fileio.cpp
  Source/UI/font.cpp
  Source/UI/input.cpp
  Source/UI/main.cpp
  Source/UI/menu.cpp
  Source/UI/menuitem.cpp
  Source/UI/menulist.cpp
  Source/UI/platform.cpp
  Source/UI/setting.cpp
  Source/UI/softkey.cpp
  Source/UI/tapemgr.cpp
  Source/UI/video.cpp
  Source/ePC-8801MA/common.cpp
  Source/ePC-8801MA/config.cpp
  Source/ePC-8801MA/fifo.cpp
  Source/ePC-8801MA/vm/disk.cpp
  Source/ePC-8801MA/vm/event.cpp
  Source/ePC-8801MA/vm/i8251.cpp
  Source/ePC-8801MA/vm/i8253.cpp
  Source/ePC-8801MA/vm/i8255.cpp
  Source/ePC-8801MA/vm/disksub.cpp
  Source/ePC-8801MA/vm/pcm1bit.cpp
  Source/ePC-8801MA/vm/upd765a.cpp
  Source/ePC-8801MA/vm/upd1990a.cpp
  Source/ePC-8801MA/vm/fmsound.cpp
  Source/ePC-8801MA/vm/z80.cpp
  Source/ePC-8801MA/vm/fmgen/opna.cpp
  Source/ePC-8801MA/vm/fmgen/psg.cpp
  Source/ePC-8801MA/vm/fmgen/fmgen.cpp
  Source/ePC-8801MA/vm/fmgen/fmtimer.cpp
  Source/ePC-8801MA/vm/pc8801/pc88.cpp
  Source/ePC-8801MA/vm/pc8801/pc8801.cpp
)

set(BIN_TARGET "xm8")
add_executable(${BIN_TARGET} MACOSX_BUNDLE ${SRCS} Platforms/macOS/AppIcon.icns)

if(CMAKE_STRIP)
  add_custom_command(
    TARGET ${BIN_TARGET} POST_BUILD
    DEPENDS ${BIN_TARGET}
    COMMAND $<$<CONFIG:Release>:${CMAKE_STRIP}>
            $<$<CONFIG:MinSizeRel>:${CMAKE_STRIP}>
    ARGS $<TARGET_FILE:${BIN_TARGET}>)
endif()

if(APPLE)
  set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/Platforms/macOS/AppIcon.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(${BIN_TARGET} PROPERTIES
      MACOSX_BUNDLE_GUI_IDENTIFIER "com.bubio.xm8"
      MACOSX_BUNDLE_COPYRIGHT "Copyright (C) 2015 PI."
      MACOSX_BUNDLE_BUNDLE_NAME "XM8"
      MACOSX_BUNDLE_DISPLAY_NAME "XM8"
      MACOSX_BUNDLE_INFO_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
      MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
      MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}"
      MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
      MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Platforms/macOS/Info.plist"
  )
  set(CMAKE_OSX_DEPLOYMENT_TARGET "10.9.0")
  option(MACOSX_STANDALONE_APP_BUNDLE "Generate a portable app bundle to use on other devices (requires sudo)" OFF)

  install (TARGETS ${BIN_TARGET} DESTINATION ./)

  if(MACOSX_STANDALONE_APP_BUNDLE)
      install(CODE "
        include(BundleUtilities)
        fixup_bundle(${CMAKE_BINARY_DIR}/${MACOSX_BUNDLE_BUNDLE_NAME}.app \"\" \"\")
        "
        COMPONENT Runtime)
  endif()

endif()

set(CPACK On)

# Set link options
if(APPLE)
  target_link_libraries(${BIN_TARGET} PRIVATE
                        ${SDL2_LIBRARIES}
                        iconv)
else()
  target_link_libraries(${BIN_TARGET} PRIVATE
                        ${SDL2_LIBRARIES})
endif()


# Packaging                      
if(CPACK)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    string(TOLOWER ${PROJECT_NAME} project_name)
    set(CPACK_PACKAGE_NAME ${project_name})
    set(CPACK_GENERATOR "7Z")
    # Common *nix files
    set(CPACK_STRIP_FILES TRUE)
    install(TARGETS ${BIN_TARGET} DESTINATION bin)
    set(desktop_file "${PROJECT_SOURCE_DIR}/Platforms/Linux/${project_name}.desktop")

    find_program(DFI desktop-file-install)
    if(DFI)
      execute_process(COMMAND ${DFI} --dir=${CMAKE_BINARY_DIR} ${desktop_file})
      set(desktop_file "${CMAKE_BINARY_DIR}/${project_name}.desktop")
    endif()

    install(FILES "${desktop_file}"
      DESTINATION "share/applications"
    )
    install(FILES "${PROJECT_SOURCE_DIR}/Platforms/Linux/icon.png"
      DESTINATION "share/icons/hicolor/512x512/apps"
      RENAME "${project_name}.png"
    )

    # -G DEB
    set(CPACK_PACKAGE_CONTACT "bubio")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "PC-8801MA Emulator\neXellent Multi-platform emulator type 8 - 'XM8' based on ePC-8801MA")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libsdl2-2.0-0")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

    # -G RPM
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

    find_program(RPMBUILD rpmbuild)
    if(RPMBUILD)
      list(APPEND CPACK_GENERATOR "RPM")
    endif()
    find_program(DPKG dpkg)
    if(DPKG)
      list(APPEND CPACK_GENERATOR "DEB")
    endif()

  elseif(APPLE)
    set(CPACK_PACKAGE_FILE_NAME "XM8")
    set(CPACK_DMG_DISABLE_APPLICATIONS_SYMLINK "ON")
    set(CPACK_STRIP_FILES TRUE)
    set(CPACK_GENERATOR "DragNDrop")
  endif()

  set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
  include(CPack)
endif()