LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := cust_clocks
LOCAL_MODULE_TAGS := nvidia_tests
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := cust_clocks
include $(NVIDIA_PREBUILT)

ifneq ($(HAVE_NVIDIA_PROP_SRC),false)
ifeq ($(TARGET_BOARD_PLATFORM),tegra)
include $(call all-makefiles-under,$(LOCAL_PATH))
endif
endif

