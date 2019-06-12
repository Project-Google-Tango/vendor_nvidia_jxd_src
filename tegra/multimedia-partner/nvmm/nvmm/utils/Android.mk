LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvmm_utils

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/2d/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia/utils/include

LOCAL_SRC_FILES += nvmm_util.c
LOCAL_SRC_FILES += nvmm_ulp_util.c
LOCAL_SRC_FILES += nvmm_ulp_kpi_logger.c
LOCAL_SRC_FILES += nvmm_bufmgr.c
LOCAL_SRC_FILES += nvmm_queue.c
LOCAL_SRC_FILES += nvmm_mediaclock.c
LOCAL_SRC_FILES += nvmm_sock_util.c
LOCAL_SRC_FILES += nvmm_file_util.c
LOCAL_SRC_FILES += nvmm_format_scan.c
LOCAL_SRC_FILES += nvmetafile_m3uparser.c
LOCAL_SRC_FILES += nvmetafile_asxparser.c
LOCAL_SRC_FILES += nvmetafile_parser.c
LOCAL_SRC_FILES += nvmm_logger.c
LOCAL_SRC_FILES += nvmm_logger_android.c
LOCAL_SRC_FILES += nvmm_sock_linux.c

LOCAL_SHARED_LIBRARIES += libnvos
LOCAL_SHARED_LIBRARIES += libnvrm
LOCAL_SHARED_LIBRARIES += libnvrm_graphics
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnvavp
LOCAL_SHARED_LIBRARIES += libnvfusebypass

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
LOCAL_CFLAGS += -Wno-error=sign-compare
LOCAL_CFLAGS += -Wno-error=switch
LOCAL_CFLAGS += -DNV_USE_NVAVP=1

include $(NVIDIA_SHARED_LIBRARY)

include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvmm_logger

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/nvmm/nvmm/include

LOCAL_SRC_FILES += nvmm_logger.c
LOCAL_SRC_FILES += nvmm_logger_android.c

LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SHARED_LIBRARIES += libnvos

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
LOCAL_CFLAGS += -Wno-error=sign-compare
include $(NVIDIA_SHARED_LIBRARY)
