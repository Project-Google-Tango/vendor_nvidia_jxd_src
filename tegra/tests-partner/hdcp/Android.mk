LOCAL_PATH := $(call my-dir)

include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
	libhdcp_up \
	nvhdcp_test \
))
