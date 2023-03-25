#
# eXellent Multi-platform emulator type 8 - 'XM8'
# based on ePC-8801MA
#
# Author (ePC-8801MA) : Takeda.Toshiya
# Author (XM8) : Tanaka.Yasushi
#
# [ makefile for macOS ]
#

#
# tool chain
#

CXX = clang

STRIP = strip

#
# options for SDL
#

SDL_CFLAGS := $(shell sdl2-config --cflags)

SDL_LDFLAGS := $(shell sdl2-config --libs)

#
# compile options
#

CXXFLAGS = -stdlib=libc++ -DSDL -D_PC8801MA -c -O2 -Wall \
			-mmacosx-version-min=10.7 \
			-Wno-macro-redefined \
			-Wno-non-c-typedef-for-linkage \
			-Wno-deprecated-declarations \
			-Wno-overloaded-virtual \
			-Wno-c++11-compat-deprecated-writable-strings

#
# include directories
#

INCFLAGS = -I../UI -I../ePC-8801MA -I../ePC-8801MA/vm -I../ePC-8801MA/vm/pc8801 -I../ePC-8801MA/vm/fmgen

#
# default rule
#
.cpp.o: 
	$(CXX) $(CXXFLAGS) $(SDL_CFLAGS) $(INCFLAGS) $< -o $@

#
# target name
#
TARGET = xm8

#
# source files
#
SRCS = \
	../UI/app.cpp \
	../UI/audio.cpp \
	../UI/converter.cpp \
	../UI/diskmgr.cpp \
	../UI/emu.cpp \
	../UI/emu_sdl.cpp \
	../UI/file.cpp \
	../UI/fileio.cpp \
	../UI/font.cpp \
	../UI/input.cpp \
	../UI/main.cpp \
	../UI/menu.cpp \
	../UI/menuitem.cpp \
	../UI/menulist.cpp \
	../UI/platform.cpp \
	../UI/setting.cpp \
	../UI/softkey.cpp \
	../UI/tapemgr.cpp \
	../UI/video.cpp \
	../ePC-8801MA/common.cpp \
	../ePC-8801MA/config.cpp \
	../ePC-8801MA/fifo.cpp \
	../ePC-8801MA/vm/disk.cpp \
	../ePC-8801MA/vm/event.cpp \
	../ePC-8801MA/vm/i8251.cpp \
	../ePC-8801MA/vm/i8253.cpp \
	../ePC-8801MA/vm/i8255.cpp \
	../ePC-8801MA/vm/disksub.cpp \
	../ePC-8801MA/vm/pcm1bit.cpp \
	../ePC-8801MA/vm/upd765a.cpp \
	../ePC-8801MA/vm/upd1990a.cpp \
	../ePC-8801MA/vm/fmsound.cpp \
	../ePC-8801MA/vm/z80.cpp \
	../ePC-8801MA/vm/fmgen/opna.cpp \
	../ePC-8801MA/vm/fmgen/psg.cpp \
	../ePC-8801MA/vm/fmgen/fmgen.cpp \
	../ePC-8801MA/vm/fmgen/fmtimer.cpp \
	../ePC-8801MA/vm/pc8801/pc88.cpp \
	../ePC-8801MA/vm/pc8801/pc8801.cpp

OBJS = $(SRCS:%.cpp=%.o)

#
# make or make all:
# make target
#
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $(TARGET) $(OBJS) $(SDL_LDFLAGS) -lc++ -liconv -mmacosx-version-min=10.7
	$(STRIP) $(TARGET)

#
# make clean:
# clean object and executable file
#
clean:
	rm $(OBJS)
	rm $(TARGET)


FRAMEWORK_PATH=$(HOME)/Library/Frameworks
APP_NAME=XM8
VERSION=1.7.1
APP_ICON=AppIcon.png

package_app:
	rm -rf "./$(APP_NAME).app/"
	bash iconcreate.sh $(APP_ICON)
	mkdir -p "./$(APP_NAME).app"/Contents/{Frameworks,MacOS,Resources}
	# cp -R "$(FRAMEWORK_PATH)/SDL2.framework" "./$(APP_NAME).app/Contents/Resources/"
	cp Info.plist "./$(APP_NAME).app/Contents/"
	sed -e "s/APP_NAME/$(APP_NAME)/g" -i "" "./$(APP_NAME).app/Contents/Info.plist"
	sed -e "s/VERSION/$(VERSION)/g" -i "" "./$(APP_NAME).app/Contents/Info.plist"
	mv ./xm8 "./$(APP_NAME).app/Contents/MacOS/$(APP_NAME)"
	mv ./AppIcon.icns "./$(APP_NAME).app/Contents/Resources/"