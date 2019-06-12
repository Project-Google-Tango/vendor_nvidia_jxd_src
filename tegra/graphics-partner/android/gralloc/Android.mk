LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := \
	liblog \
	libcutils \
	libdl \
	libsync \
	libEGL \
	libnvgr \
	libnvos \
	libnvrm \
	libnvrm_graphics \
	libnvblit \
	libsync
LOCAL_STATIC_LIBRARIES := \
	libnvfxmath
LOCAL_SRC_FILES := \
	nvgrmodule.c \
	nvgralloc.c \
	nvgrbuffer.c \
	nvgr_scratch.c \
	nvgr_2d.c \
	nvgr_egl.c \
	nvgr_props.c
LOCAL_MODULE := gralloc.$(TARGET_BOARD_PLATFORM)

LOCAL_CFLAGS += -DLOG_TAG=\"gralloc\"

LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics-partner/android/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/2d/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/color/nvcms/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include
LOCAL_C_INCLUDES += $(NV_GPUDRV_SOURCE)/drivers/khronos/interface/partner

# This config can be used to reduce memory usage at the cost of performance.
ifeq ($(BOARD_DISABLE_TRIPLE_BUFFERED_DISPLAY_SURFACES),true)
    LOCAL_CFLAGS += -DNVGR_USE_TRIPLE_BUFFERING=0
else
    LOCAL_CFLAGS += -DNVGR_USE_TRIPLE_BUFFERING=1
endif

ifeq ($(BOARD_SUPPORT_MHL_CTS), 1)
    LOCAL_CFLAGS += -DSUPPORT_MHL_CTS=1
else
    LOCAL_CFLAGS += -DSUPPORT_MHL_CTS=0
endif

ifeq ($(BOARD_ENABLE_NV_ATRACE),true)
    LOCAL_CFLAGS += -DNV_ATRACE=1
endif

include $(NVIDIA_SHARED_LIBRARY)
