LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

SDL_PATH := ../SDL

XM8_SRC_PATH := $(LOCAL_PATH)/../../../../../Source

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/$(SDL_PATH)/include \
	$(LOCAL_PATH) \
	$(XM8_SRC_PATH)/UI \
	$(XM8_SRC_PATH)/ePC-8801MA \
	$(XM8_SRC_PATH)/ePC-8801MA/vm \
	$(XM8_SRC_PATH)/ePC-8801MA/vm/fmgen \
	$(XM8_SRC_PATH)/ePC-8801MA/vm/pc8801 \
	$(XM8_SRC_PATH)/Filter/xBRZ

ifeq ($(XM8_ENABLE_RETROACHIEVEMENTS),1)
LOCAL_C_INCLUDES += \
	$(XM8_SRC_PATH)/RA \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/include \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rcheevos \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rapi \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rhash \
	$(LOCAL_PATH)/../../../../../ThirdParty/sqlite \
	$(LOCAL_PATH)/../../../../../ThirdParty/stb
endif

# Add your application source files here...
LOCAL_SRC_FILES := \
    $(wildcard $(XM8_SRC_PATH)/ePC-8801MA/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/ePC-8801MA/vm/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/ePC-8801MA/vm/fmgen/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/ePC-8801MA/vm/pc8801/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/Filter/xBRZ/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/UI/*.cpp) \
    $(wildcard $(LOCAL_PATH)/*.c)

ifeq ($(XM8_ENABLE_RETROACHIEVEMENTS),1)
LOCAL_SRC_FILES += \
	$(filter-out $(XM8_SRC_PATH)/RA/ra_http_linux.cpp $(XM8_SRC_PATH)/RA/ra_http_win.cpp $(XM8_SRC_PATH)/RA/ra_connectivity_linux.cpp $(XM8_SRC_PATH)/RA/ra_connectivity_win.cpp $(XM8_SRC_PATH)/RA/ra_connectivity_mac.cpp $(XM8_SRC_PATH)/RA/ra_http_fake.cpp,$(wildcard $(XM8_SRC_PATH)/RA/*.cpp)) \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rc_client.c \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rc_client_raintegration.c \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rc_compat.c \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rc_util.c \
	$(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rc_version.c \
	$(wildcard $(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rcheevos/*.c) \
	$(wildcard $(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rapi/*.c) \
	$(wildcard $(LOCAL_PATH)/../../../../../ThirdParty/rcheevos/src/rhash/*.c) \
	$(LOCAL_PATH)/../../../../../ThirdParty/sqlite/sqlite3.c
endif

LOCAL_SHARED_LIBRARIES := SDL2

LOCAL_CFLAGS := -DSDL -D_PC8801MA -D__ANDROID__ -Wno-narrowing -std=c17
LOCAL_CPPFLAGS := -DSDL -D_PC8801MA -D__ANDROID__ -Wno-narrowing -std=c++17
ifeq ($(XM8_ENABLE_RETROACHIEVEMENTS),1)
LOCAL_CFLAGS += -DXM8_ENABLE_RETROACHIEVEMENTS=1 -DRC_DISABLE_LUA -DRC_CLIENT_SUPPORTS_HASH -DSQLITE_THREADSAFE=1 -DSQLITE_DEFAULT_FOREIGN_KEYS=1 -DSQLITE_OMIT_LOAD_EXTENSION -DSQLITE_DQS=0
LOCAL_CPPFLAGS += -DXM8_ENABLE_RETROACHIEVEMENTS=1 -DRC_DISABLE_LUA -DRC_CLIENT_SUPPORTS_HASH
endif
LOCAL_CPP_FEATURES := exceptions
LOCAL_LDFLAGS := -Wl,-z,max-page-size=16384

# LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog
LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -lOpenSLES -llog -landroid

include $(BUILD_SHARED_LIBRARY)
