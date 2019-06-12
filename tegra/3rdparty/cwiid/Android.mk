# Build this only is BlueTooth is available
ifneq ($(PLATFORM_IS_JELLYBEAN_MR1),1)
ifeq ($(strip $(BOARD_HAVE_BLUETOOTH)),true)
LOCAL_PATH := $(my-dir)
subdir_makefiles := \
	$(LOCAL_PATH)/libcwiid/Android.mk \
	$(LOCAL_PATH)/wminput/Android.mk \
	$(LOCAL_PATH)/wminput/plugins/Android.mk
include $(subdir_makefiles)
endif
endif
