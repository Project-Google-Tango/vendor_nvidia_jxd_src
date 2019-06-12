LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)
include $(LOCAL_PATH)/../../Android.common.mk

LOCAL_MODULE := libnv_parser

LOCAL_C_INCLUDES += $(TEGRA_TOP)/multimedia-partner/openmax/include/openmax/il
LOCAL_CFLAGS += -Werror

LOCAL_SRC_FILES += nvmm_super_parserblock.c
LOCAL_SRC_FILES += parser_core/mp3/nvmm_mp3parser_core.c
LOCAL_SRC_FILES += parser_core/mp3/nvmm_mp3parser.c
LOCAL_SRC_FILES += parser_core/mp4/nvmm_mp4parser_core.c
LOCAL_SRC_FILES += parser_core/mp4/nv_mp4parser.c
LOCAL_SRC_FILES += parser_core/ogg/nvmm_oggparser_core.c
LOCAL_SRC_FILES += parser_core/ogg/nvmm_oggparser.c
LOCAL_SRC_FILES += parser_core/ogg/nvmm_oggbookunpack.c
LOCAL_SRC_FILES += parser_core/ogg/nvmm_oggframing.c
LOCAL_SRC_FILES += parser_core/ogg/nvmm_ogginfo.c
LOCAL_SRC_FILES += parser_core/ogg/nvmm_oggwrapper.c
LOCAL_SRC_FILES += parser_core/ogg/nvmm_oggvorbisfile.c
LOCAL_SRC_FILES += parser_core/ogg/nvmm_oggseek.c
LOCAL_SRC_FILES += parser_core/aac/nvmm_aacparser_core.c
LOCAL_SRC_FILES += parser_core/aac/nvmm_aacparser.c
LOCAL_SRC_FILES += parser_core/amr/nvmm_amrparser_core.c
LOCAL_SRC_FILES += parser_core/amr/nvmm_amrparser.c
LOCAL_SRC_FILES += parser_core/m2v/nvmm_m2vparser_core.c
LOCAL_SRC_FILES += parser_core/m2v/nvmm_m2vparser.c
LOCAL_SRC_FILES += parser_core/nem/nvmm_nemparser_core.c
LOCAL_SRC_FILES += parser_core/wav/nvmm_wavparser_core.c
LOCAL_SRC_FILES += parser_core/wav/nvmm_wavparser.c
LOCAL_SRC_FILES += parser_core/mps/nvmm_mpsparser_core.c
LOCAL_SRC_FILES += parser_core/mps/nvmm_mps_parser.c
LOCAL_SRC_FILES += parser_core/mps/nvmm_mps_reader.c

LOCAL_CFLAGS += -Wno-error=enum-compare
include $(NVIDIA_STATIC_LIBRARY)

