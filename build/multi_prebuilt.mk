#
# Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
#

# For multi prebuilts, there could be multiple prebuilts defined
# same section, need to seperate them and add them to ALL_NVIDIA_MODULES

define multi-prebuilt-handler
$(foreach t,$(1), \
  $(eval tw := $(subst :, ,$(strip $(t)))) \
  $(if $(word 2,$(tw)), \
  $(eval ALL_NVIDIA_MODULES += $(word 1,$(tw))) \
  , \
  $(eval ALL_NVIDIA_MODULES += $(basename $(notdir $(t)))) \
  ) \
)
endef

ifneq ($(LOCAL_PREBUILT_LIBS),)
$(call multi-prebuilt-handler, $(LOCAL_PREBUILT_LIBS))
endif
ifneq ($(LOCAL_PREBUILT_EXECUTABLES),)
$(call multi-prebuilt-handler, $(LOCAL_PREBUILT_EXECUTABLES))
endif
ifneq ($(LOCAL_PREBUILT_JAVA_LIBRARIES),)
$(call multi-prebuilt-handler, $(LOCAL_PREBUILT_JAVA_LIBRARIES))
endif
ifneq ($(LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES),)
$(call multi-prebuilt-handler, $(LOCAL_PREBUILT_STATIC_JAVA_LIBRARIES))
endif

include $(BUILD_MULTI_PREBUILT)
