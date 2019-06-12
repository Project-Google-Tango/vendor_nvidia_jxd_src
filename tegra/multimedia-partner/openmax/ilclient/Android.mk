LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvomxilclient
LOCAL_NO_DEFAULT_COMPILER_FLAGS := true

LOCAL_STATIC_LIBRARIES += libnvfxmath

LOCAL_CFLAGS += $(TARGET_GLOBAL_CFLAGS)
LOCAL_CFLAGS += $(TARGET_thumb_CFLAGS)
LOCAL_CFLAGS += -D__OMX_EXPORTS
LOCAL_CFLAGS += -DOMXVERSION=2

ifeq ($(NV_LOGGER_ENABLED),1)
LOCAL_CFLAGS += -DNV_LOGGER_ENABLED=1
endif

LOCAL_C_INCLUDES += $(TARGET_PROJECT_INCLUDES)
LOCAL_C_INCLUDES += $(TARGET_C_INCLUDES)
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/ilclient
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il

LOCAL_SRC_FILES += nvxrecordgraph.c
LOCAL_SRC_FILES += nvxtunneledrecordgraph.c
LOCAL_SRC_FILES += nvxframework.c
LOCAL_SRC_FILES += nvxgraph.c
LOCAL_SRC_FILES += nvxplayergraph.c


LOCAL_SHARED_LIBRARIES += libnvos
ifeq ($(NV_LOGGER_ENABLED),1)
LOCAL_SHARED_LIBRARIES += libnvmm_logger
endif

include $(NVIDIA_SHARED_LIBRARY)

