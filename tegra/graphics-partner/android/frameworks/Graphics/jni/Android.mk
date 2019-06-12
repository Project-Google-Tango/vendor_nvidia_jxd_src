LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE:= libnvidia_graphics_jni

LOCAL_SRC_FILES:= \
	com_nvidia_graphics_blit.cpp \
	com_nvidia_graphics_onload.cpp

LOCAL_SHARED_LIBRARIES := \
	libandroid_runtime \
	libcutils \
	libgui \
	libnvblit \
	libnvgr \
	libnvos \
	libsync \
	libui \
	libutils

CPPLIB_PATH := $(TOP)/prebuilts/ndk/8/sources/cxx-stl/gnu-libstdc++

LOCAL_C_INCLUDES += \
	$(JNI_H_INCLUDE) \
	$(TEGRA_TOP)/core/include \
	$(TEGRA_TOP)/graphics/2d/include \
	$(TEGRA_TOP)/graphics/color/nvcms/include \
	$(TEGRA_TOP)/graphics-partner/android/include \
	$(CPPLIB_PATH)/include \
	$(CPPLIB_PATH)/libs/armeabi-v7a/include

LOCAL_CFLAGS += -frtti -fexceptions
LOCAL_LDFLAGS += -Wl,--start-group $(CPPLIB_PATH)/libs/armeabi-v7a/libgnustl_static.a -Wl,--end-group

# Don't prelink this library.  For more efficient code, you may want
# to add this library to the prelink map and set this to true.
LOCAL_PRELINK_MODULE := false

include $(NVIDIA_SHARED_LIBRARY)
