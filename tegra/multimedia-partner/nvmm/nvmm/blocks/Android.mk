LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)
include $(LOCAL_PATH)/Android.blocks.mk

LOCAL_MODULE := libnvmm_writer
LOCAL_SRC_FILES += common/nvmm_block.c
LOCAL_SHARED_LIBRARIES += libnvmm_utils
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvmm_contentpipe

BLOCKLIBS :=
BLOCKLIBS += libnvbasewriter
ifeq ($(3GPWRITER_BUILT),1)
BLOCKLIBS += libnv3gpwriter
endif

LOCAL_WHOLE_STATIC_LIBRARIES += $(sort $(BLOCKLIBS))

include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)
include $(LOCAL_PATH)/Android.blocks.mk

LOCAL_MODULE := libnvmm_parser
LOCAL_SRC_FILES += common/nvmm_block.c
LOCAL_SHARED_LIBRARIES += libnvmm_utils
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvmm_contentpipe
LOCAL_WHOLE_STATIC_LIBRARIES += libnv_parser

include $(NVIDIA_SHARED_LIBRARY)

BLOCKDIRS += common
BLOCKDIRS += writer_common
BLOCKDIRS += super_parser

include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, $(sort $(BLOCKDIRS))))
