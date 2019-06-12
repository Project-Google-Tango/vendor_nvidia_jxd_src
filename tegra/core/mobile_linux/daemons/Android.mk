LOCAL_PATH := $(call my-dir)

daemon_list := \
	nv_hciattach

include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
	$(daemon_list) \
))
