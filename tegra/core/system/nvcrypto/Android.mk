#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

LOCAL_PATH := $(call my-dir)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvcrypto


LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/aes_ref
LOCAL_NVIDIA_NO_COVERAGE := true
LOCAL_SRC_FILES += nvcrypto_padding.c
LOCAL_SRC_FILES += nvcrypto_cipher.c
LOCAL_SRC_FILES += nvcrypto_cipher_aes.c
LOCAL_SRC_FILES += nvcrypto_hash.c
LOCAL_SRC_FILES += nvcrypto_hash_cmac.c
LOCAL_SRC_FILES += nvcrypto_random.c
LOCAL_SRC_FILES += nvcrypto_random_ansix931.c

ifeq ($(filter t30, $(TARGET_TEGRA_VERSION)),)
LOCAL_CFLAGS += -DNV_IS_SE_USED=1

ifneq ($(BOOT_MINIMAL_BL),1)
LOCAL_SRC_FILES += nvcrypto_cipher_se_aes.c
else
LOCAL_CFLAGS += -DBOOT_MINIMAL_BL
LOCAL_SRC_FILES += nvcrypto_cipher_sw_aes.c
endif

else
LOCAL_CFLAGS += -DNV_IS_SE_USED=0
endif

LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1
include $(NVIDIA_STATIC_LIBRARY)
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE := libnvswcrypto
LOCAL_C_INCLUDES += $(TEGRA_TOP)/core/utils/aes_ref

LOCAL_SRC_FILES += nvcrypto_padding.c
LOCAL_SRC_FILES += nvcrypto_cipher.c
LOCAL_SRC_FILES += nvcrypto_hash.c
LOCAL_SRC_FILES += nvcrypto_hash_cmac.c
LOCAL_SRC_FILES += nvcrypto_random.c
LOCAL_SRC_FILES += nvcrypto_cipher_sw_aes.c

include $(NVIDIA_HOST_STATIC_LIBRARY)
