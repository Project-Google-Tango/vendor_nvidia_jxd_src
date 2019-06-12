# icefsd/Android.mk
#
# Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
#

# This is the Icera file system server application

LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE:= fild
LOCAL_SRC_FILES += \
       fild.c \
       fild_boot.c \
       fild_fs.c \
       fild_port.c \
       obex_file.c \
       obex_transport.c \
       obex_core.c \
       obex_server.c \
       obex_port.c \
       fild_forward.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/../icera-util

LOCAL_SHARED_LIBRARIES := \
	libcutils

LOCAL_STATIC_LIBRARIES := \
    libutil-icera

include $(NVIDIA_EXECUTABLE)