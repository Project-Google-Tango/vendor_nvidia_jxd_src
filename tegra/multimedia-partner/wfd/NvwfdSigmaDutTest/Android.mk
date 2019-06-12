# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH:= $(call my-dir)
include $(NVIDIA_DEFAULTS)
#
# SIGMA DUT START TEST
#

LOCAL_SHARED_LIBRARIES := libwfd_source

LOCAL_SRC_FILES:= NvwfdSigmaDutTest.c

LOCAL_MODULE := NvwfdSigmaDutTest

include $(NVIDIA_EXECUTABLE)

