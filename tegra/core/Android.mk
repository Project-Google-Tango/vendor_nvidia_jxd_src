ifneq ($(HAVE_NVIDIA_PROP_SRC),false)
LOCAL_PATH := $(call my-dir)

include $(LOCAL_PATH)/modules.mk

include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
	drivers \
	include \
	system \
	utils \
	mobile_linux/daemons \
))
endif
