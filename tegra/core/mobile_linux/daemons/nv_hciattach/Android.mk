LOCAL_PATH := $(call my-dir)

# nv_hciattach executable

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nv_hciattach

LOCAL_SRC_FILES +=nv_hciattach.c

include $(NVIDIA_EXECUTABLE)
