LOCAL_PATH := $(call my-dir)

_local_subdirs := nvlimits nvrmkernel graphics secure dgpu

include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, $(_local_subdirs)))

