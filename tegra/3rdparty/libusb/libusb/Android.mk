LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)
LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1

LOCAL_SRC_FILES:= \
 core.c \
 descriptor.c \
 io.c \
 sync.c \
 os/linux_usbfs.c

LOCAL_C_INCLUDES += \
vendor/nvidia/tegra/3rdparty/libusb/ \
vendor/nvidia/tegra/3rdparty/libusb/libusb/ \
vendor/nvidia/tegra/3rdparty/libusb/libusb/os

LOCAL_MODULE_TAGS := optional

LOCAL_MODULE:= libusb
include $(NVIDIA_SHARED_LIBRARY)
