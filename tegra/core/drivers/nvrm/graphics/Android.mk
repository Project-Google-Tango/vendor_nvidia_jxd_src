LOCAL_PATH := $(call my-dir)

# libnvrm_graphics (channel & moduleloader stubs)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvrm_graphics
LOCAL_C_INCLUDES += $(LOCAL_PATH)/t30
LOCAL_C_INCLUDES += $(LOCAL_PATH)/t14x
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc-private
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../nvrmkernel/core/common
LOCAL_C_INCLUDES += $(TOP)/system/core/include

LOCAL_SRC_FILES += nvrm_disasm.c
LOCAL_SRC_FILES += nvrm_stream.c
LOCAL_SRC_FILES += nvsched.c
LOCAL_SRC_FILES += nvrm_channel_linux.c

LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libsync

# XXX Can explicit listing of export files be removed?  Old comment
# suggested it was only needed for old csim builds.
LOCAL_NVIDIA_EXPORTS := libnvrm_graphics.export

include $(NVIDIA_SHARED_LIBRARY)

# libnvrm_channel_impl for bootloader

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvrm_channel_impl
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_C_INCLUDES += $(TEGRA_TOP)/hwinc-private/$(TARGET_TEGRA_FAMILY)
LOCAL_CFLAGS += -DNVRM_TRANSPORT_IN_KERNEL=1

LOCAL_SRC_FILES += nvrm_disasm.c
LOCAL_SRC_FILES += nvrm_stream.c
LOCAL_SRC_FILES += nvsched.c
LOCAL_SRC_FILES += nvrm_graphics_init.c
LOCAL_SRC_FILES += host1x/host1x_intr.c
LOCAL_SRC_FILES += host1x/host1x_hwcontext.c
LOCAL_SRC_FILES += host1x/host1x_channel.c

include $(NVIDIA_STATIC_LIBRARY)
