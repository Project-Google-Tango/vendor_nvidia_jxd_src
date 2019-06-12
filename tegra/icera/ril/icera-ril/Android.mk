# Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.

# Copyright 2006 The Android Open Source Project

# XXX using libutils for simulator build only...
#

LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := \
    atchannel.c \
    at_channel_handle.c \
    at_channel_writer.c \
    at_channel_config.c \
    ril_request_queue.c \
    ril_request_object.c \
    ril_unsol_object.c \
    misc.c \
    at_tok.c \
    icera-ril-net.c \
    icera-ril-data.c \
    icera-ril-call.c \
    icera-ril-sms.c \
    icera-ril-sim.c \
    icera-ril-stk.c \
    icera-ril-services.c \
    icera-ril-utils.c \
    icera-ril-logging.c \
    icera-ril-fwupdate.c \
    icera-ril-power.c \
    icera-ril.c

LOCAL_SHARED_LIBRARIES := \
    libcutils libutils libril libnetutils

LOCAL_STATIC_LIBRARIES := \
    libutil-icera

# for asprinf
LOCAL_CFLAGS := -D_GNU_SOURCE

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)

LOCAL_C_INCLUDES += \
    $(LOCAL_PATH)/../icera-util \
    $(TOP)/external/openssl/include

ifeq ($(TARGET_BUILD_TYPE),debug)
    LOCAL_CFLAGS += -DESCAPE_IN_ECCLIST
endif

LOCAL_CFLAGS += -DRIL_SHLIB
LOCAL_MODULE:= libril-icera-core

intermediates := $(call intermediates-dir-for,STATIC_LIBRARIES,$(LOCAL_MODULE))
# $(all_objects) depends on this -> version.h is generated before it is needed
LOCAL_GENERATED_SOURCES += $(intermediates)/new-version.h

include $(NVIDIA_STATIC_LIBRARY)

# Get the version of the RIL
# new-version.h: always generated
# version.h    : only modified when it is different from new-version.h
.PHONY: $(intermediates)/new-version.h
$(intermediates)/new-version.h: PRIVATE_PATH := $(LOCAL_PATH)
$(intermediates)/new-version.h: | $(intermediates)
	$(hide)set -e; \
	v=$$(git --git-dir=$(PRIVATE_PATH)/../.git --work-tree=$(PRIVATE_PATH)/.. log --oneline -1 | tr -d '"'); \
	f=$$(git --git-dir=$(PRIVATE_PATH)/../.git --work-tree=$(PRIVATE_PATH)/.. status | grep modified | tr -d '\t' | sed 's/\#/\\\\n/' |tr -d '\n'); \
	echo >$@ -e "#define RIL_VERSION_DETAILS \"$$v\"\\n#define RIL_MODIFIED_FILES \"$$f\""; \
	cmp -s $@ $(dir $@)version.h || cp $@ $(dir $@)version.h

$(intermediates):
	$(hide)mkdir -p $@

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional

LOCAL_SRC_FILES := icera-ril-extension-stub.c

LOCAL_WHOLE_STATIC_LIBRARIES := \
    libutil-icera libril-icera-core

ifeq (foo,foo)
    #build shared library
    LOCAL_SHARED_LIBRARIES += \
        libcutils libutils libnetutils libril libcrypto librilutils

    LOCAL_MODULE := libril-icera

    include $(NVIDIA_SHARED_LIBRARY)
else
    #build executable
    LOCAL_SHARED_LIBRARIES += \
        libril

    LOCAL_MODULE := reference-ril

    include $(BUILD_EXECUTABLE)
endif
