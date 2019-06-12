#
# Copyright (c) 2012-2014, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#
LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvodm_imager
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_PRELINK_MODULE := false
ifeq ($(REFERENCE_DEVICE),t132)
PLATFORM_PATH := vendor/nvidia/tegra/camera-partner/imager/platform/ardbeg
else
PLATFORM_PATH := vendor/nvidia/tegra/camera-partner/imager/platform/$(REFERENCE_DEVICE)
endif
TEMPLATE_PATH := vendor/nvidia/tegra/camera-partner/imager/platform/template

LOCAL_CFLAGS += -DBUILD_FOR_AOS=0

ifeq ($(REFERENCE_DEVICE),cardhu)
LOCAL_CFLAGS += -DBUILD_FOR_CARDHU=1
endif
ifeq ($(REFERENCE_DEVICE),ceres)
LOCAL_CFLAGS += -DBUILD_FOR_CERES=1
endif
ifeq ($(REFERENCE_DEVICE),bonaire)
LOCAL_CFLAGS += -DBUILD_FOR_BONAIRE=1
endif
ifeq ($(REFERENCE_DEVICE),bonaire_sim)
LOCAL_CFLAGS += -DBUILD_FOR_BONAIRE_SIM=1
endif
ifeq ($(REFERENCE_DEVICE),ardbeg)
LOCAL_CFLAGS += -DBUILD_FOR_ARDBEG=1
endif
ifeq ($(REFERENCE_DEVICE),loki)
LOCAL_CFLAGS += -DBUILD_FOR_LOKI=1
endif
ifeq ($(REFERENCE_DEVICE),ardbeg_t114)
LOCAL_CFLAGS += -DBUILD_FOR_ARDBEG=1
endif
ifeq ($(REFERENCE_DEVICE),dalmore)
LOCAL_CFLAGS += -DBUILD_FOR_DALMORE=1
endif
ifeq ($(REFERENCE_DEVICE),t132)
LOCAL_CFLAGS += -DBUILD_FOR_ARDBEG=1
endif

ifeq ($(NV_TARGET_BOOTLOADER_PINMUX),kernel)
LOCAL_CFLAGS += -DSET_KERNEL_PINMUX
endif

LOCAL_C_INCLUDES += $(LOCAL_PATH)/configs
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../bootloader/nvbootloader/odm-partner/template/odm_kit/query/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../odm/template/odm_kit/query/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../multimedia-partner/nvmm/nvmm/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../camera/core_v3/include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../../camera/utils/include

LOCAL_SRC_FILES += imager_hal.c
LOCAL_SRC_FILES += imager_util.c
LOCAL_SRC_FILES += imager_nvc.c
LOCAL_SRC_FILES += imager_det.c
LOCAL_SRC_FILES += imager_static.c
LOCAL_SRC_FILES += nvc_bayer_ov9772.c
LOCAL_SRC_FILES += nvc_bayer_imx091.c
LOCAL_SRC_FILES += sensor_bayer_ov5693.c
LOCAL_SRC_FILES += sensor_bayer_imx179.c
LOCAL_SRC_FILES += sensor_yuv_soc380.c
LOCAL_SRC_FILES += sensor_bayer_ar0832.c
LOCAL_SRC_FILES += sensor_yuv_ov5640.c
LOCAL_SRC_FILES += sensor_bayer_ov5650.c
LOCAL_SRC_FILES += sensor_bayer_ov9726.c
LOCAL_SRC_FILES += sensor_bayer_ov14810.c
LOCAL_SRC_FILES += sensor_bayer_ov2710.c
LOCAL_SRC_FILES += sensor_yuv_ov7695.c
LOCAL_SRC_FILES += sensor_yuv_sp2529.c
LOCAL_SRC_FILES += sensor_null.c
LOCAL_SRC_FILES += sensor_host.c
LOCAL_SRC_FILES += focuser_ad5820.c
LOCAL_SRC_FILES += focuser_ad5823.c
LOCAL_SRC_FILES += focuser_ar0832.c
LOCAL_SRC_FILES += focuser_nvc.c
LOCAL_SRC_FILES += torch_nvc.c
LOCAL_SRC_FILES += sensor_bayer_imx135.c
LOCAL_SRC_FILES += sensor_bayer_ar0261.c
LOCAL_SRC_FILES += sensor_bayer_imx132.c
LOCAL_SRC_FILES += sensor_bayer_ar0833.c
LOCAL_SRC_FILES += focuser_dw9718.c
LOCAL_SRC_FILES += sensor_yuv_mt9m114.c
LOCAL_SRC_FILES += nvpcl_controller.c
LOCAL_SRC_FILES += nvpcl_controller_funcs.c
LOCAL_SRC_FILES += nvpcl_hw_translator.c
LOCAL_SRC_FILES += nvpcl_nvodm_tools.c
LOCAL_SRC_FILES += nvpcl_prints.c
LOCAL_SRC_FILES += nvpcl_api.c

ifeq ($(wildcard $(PLATFORM_PATH)/nvodm_query_camera.c),$(PLATFORM_PATH)/nvodm_query_camera.c)
ifeq ($(REFERENCE_DEVICE),t132)
    LOCAL_SRC_FILES += platform/ardbeg/nvodm_query_camera.c
else
    LOCAL_SRC_FILES += platform/$(REFERENCE_DEVICE)/nvodm_query_camera.c
endif
else
    LOCAL_SRC_FILES += platform/template/nvodm_query_camera.c
endif

GEN := $(call local-intermediates-dir)/nvodm_imager_build_date.c
.PHONY: $(GEN)
$(GEN): PRIVATE_CUSTOM_TOOL = @echo "const char *g_BuildDate = \"" `date` "\";" > $@
$(GEN):
	$(transform-generated-source)

LOCAL_GENERATED_SOURCES := $(GEN)

LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvrm_graphics
LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvodm_query
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_STATIC_LIBRARIES += libnvodm_services

ENABLE_NVIDIA_CAMTRACE := false
ifeq ($(ENABLE_NVIDIA_CAMTRACE), true)
LOCAL_CFLAGS += -DENABLE_NVIDIA_CAMTRACE=1
LOCAL_SHARED_LIBRARIES += libnvcamtracer
LOCAL_C_INCLUDES += $(TEGRA_TOP)/camera/utils/nvcamtrace
else
LOCAL_CFLAGS += -DENABLE_NVIDIA_CAMTRACE=0
endif

BLOCKLIBS:=
BLOCKLIBS += libnvcameradata
BLOCKLIBS += libnvcameraframedata

LOCAL_WHOLE_STATIC_LIBRARIES += $(sort $(BLOCKLIBS))
#TODO: Remove following lines before include statement and fix the source giving warnings/errors
LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS := 1
LOCAL_CFLAGS += -Werror=unused-variable

include $(NVIDIA_SHARED_LIBRARY)

# Clear local variable
GEN :=
