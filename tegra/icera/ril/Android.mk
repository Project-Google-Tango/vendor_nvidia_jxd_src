# DO NOT add conditionals to this makefile of the form
#
#    ifeq ($(TARGET_TEGRA_VERSION),<latest SOC>)
#        <stuff for latest SOC>
#    endif
#
# Such conditionals break forward compatibility with future SOCs.
# If you must add conditionals to this makefile, use the form
#
#    ifneq ($(filter <list of older SOCs>,$(TARGET_TEGRA_VERSION)),)
#       <stuff for old SOCs>
#    else
#       <stuff for new SOCs>
#    endif

include $(call all-subdir-makefiles)

include $(CLEAR_VARS)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nvidia_tegra_icera_common_modules
LOCAL_MODULE_TAGS := optional
ifeq ($(TARGET_PRODUCT),roth)
LOCAL_REQUIRED_MODULES := libril-icera\
	                  Stk
else
LOCAL_REQUIRED_MODULES := libril-icera\
	                  Stk \
	                  icera_log_serial_arm \
                          icera-crashlogs \
                          icera_host_test \
                          ttyfwd \
                          nwifup \
                          nwifdown \
                          at-port-forward \
                          log-port-forward \
                          agpsd

ifeq ($(TARGET_BUILD_VARIANT), eng)
LOCAL_REQUIRED_MODULES += IceraDebugAgent
endif
endif

include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nvidia_tegra_icera_phone_modules
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := fild \
                          rsm_stats

include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := nvidia_tegra_icera_tablet_modules
LOCAL_MODULE_TAGS := optional
LOCAL_REQUIRED_MODULES := icera-switcherd \
                          downloader \
                          icera-loader \
                          crash-check-arm

include $(BUILD_PHONY_PACKAGE)
