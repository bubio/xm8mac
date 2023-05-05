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
	$(XM8_SRC_PATH)/ePC-8801MA/vm/pc8801

# Add your application source files here...
LOCAL_SRC_FILES := \
    $(wildcard $(XM8_SRC_PATH)/ePC-8801MA/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/ePC-8801MA/vm/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/ePC-8801MA/vm/fmgen/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/ePC-8801MA/vm/pc8801/*.cpp) \
    $(wildcard $(XM8_SRC_PATH)/UI/*.cpp) \
    $(wildcard $(LOCAL_PATH)/*.c)

LOCAL_SHARED_LIBRARIES := SDL2
LOCAL_CFLAGS += -DSDL -D_PC8801MA -D__ANDROID__ -Wno-narrowing
LOCAL_CPP_FEATURES += exceptions

LOCAL_LDLIBS := -lGLESv1_CM -lGLESv2 -llog

include $(BUILD_SHARED_LIBRARY)
