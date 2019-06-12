LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvcam_imageencoder

# TEGRA3 and TEGRA2 supports different HW image encoder
ifeq ($(TARGET_TEGRA_VERSION), ap20)
    LOCAL_CFLAGS += -DUSE_NVMM_ENC=1
else
    LOCAL_CFLAGS += -DUSE_NVMM_ENC=0
endif

# We really want to get things clean enough that we can check
# this in with -Werror on.
LOCAL_CFLAGS += -Werror
LOCAL_CFLAGS += -Wno-error=unused-parameter

LOCAL_SRC_FILES :=
LOCAL_SRC_FILES += nvimage_enc.c
LOCAL_SRC_FILES += nvimage_enc_exifheader.c
LOCAL_SRC_FILES += T3/enc_T3.c
LOCAL_SRC_FILES += T2/enc_T2.c
LOCAL_SRC_FILES += sw_enc/enc_sw.c
LOCAL_SRC_FILES += nvimage_enc_makernote_extension_serializer.c
LOCAL_SRC_FILES += nvimage_enc_jds.c

LOCAL_STATIC_LIBRARIES += libnvencmakernote

LOCAL_SHARED_LIBRARIES := \
	libnvos \
	libjpeg \
	libnvmm_utils \
	libnvrm \
	libnvtvmr

LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/2d/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/../../../external/jpeg
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera-partner/image_enc/T3
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera-partner/image_enc/T2
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera-partner/image_enc/sw_enc
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera-partner/image_enc/include

#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
include $(NVIDIA_SHARED_LIBRARY)
