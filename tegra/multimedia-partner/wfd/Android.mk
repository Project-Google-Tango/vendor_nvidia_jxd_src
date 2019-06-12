# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.
#
ifeq ($(TARGET_BUILD_PDK),)
ifneq ($(strip $(BOARD_USES_GENERIC_AUDIO)),true)
ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)
LOCAL_PATH := $(call my-dir)

include $(addprefix $(LOCAL_PATH)/, $(addsuffix /Android.mk, \
    nvcap_test \
    NvCapTest  \
    NvCapPermTest  \
    NvwfdServiceTest \
    NvwfdService \
    NvwfdSigmaDutTest \
))

endif
endif
endif

