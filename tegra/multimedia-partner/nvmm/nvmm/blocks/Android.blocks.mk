LOCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/../Android.common.mk

NVIDIA_MAKEFILE += $(LOCAL_PATH)/Android.blocks.mk

BLOCKDIRS :=
ifeq ($(3GPWRITER_ENABLED),1)
ifeq ($(3GPWRITER_LOCALE),$(LOCALE_CPU))
BLOCKDIRS += 3gp_writer
3GPWRITER_BUILT := 1
endif
endif

