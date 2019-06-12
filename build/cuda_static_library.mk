#
# Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
#
# Nvidia CUDA target static library
#
LOCAL_MODULE_CLASS            := STATIC_LIBRARIES
LOCAL_MODULE_SUFFIX           := .a
LOCAL_SYSTEM_SHARED_LIBRARIES :=
LOCAL_UNINSTALLABLE_MODULE    := true

ifeq ($(TARGET_IS_64_BIT),true)
    LOCAL_2ND_ARCH_VAR_PREFIX := $(TARGET_2ND_ARCH_VAR_PREFIX)
endif

# CUDA tools.  HACK!  These paths all need adjusting when we
# get the real CUDA toolkit in place.
ANDROID_CUDA_PATH=$(TOP)/vendor/nvidia/tegra/cuda-toolkit-core-6.0/cuda-6.0
CUDA_EABI=armv7-linux-androideabi
CUDA_TOOLKIT_ROOT=$(ANDROID_CUDA_PATH)/targets/$(CUDA_EABI)
LOCAL_CC := $(ANDROID_CUDA_PATH)/bin/nvcc
TMP_NDK := $(TOP)/prebuilts/ndk/9/

# We override the compiler location because the current version of NVCC & friends
# require version 4.6 of the compiler.
TMP_GPP := $(TOP)/prebuilts/gcc/linux-x86/arm/arm-linux-androideabi-4.6

# Override standard C flags
_local_cuda_cflags :=
_local_cuda_cflags += -target-cpu-arch=ARM
_local_cuda_cflags += -m32
_local_cuda_cflags += -arch=compute_32
_local_cuda_cflags += -code=sm_32
_local_cuda_cflags += --use_fast_math
_local_cuda_cflags += -O3
_local_cuda_cflags += -Xptxas '-dlcm=ca'
_local_cuda_cflags += -target-os-variant=Android
#_local_cuda_cflags += -Xcompiler="-mtune=cortex-a9 -march=armv7-a -mfloat-abi=softfp -mfpu=neon"
_local_cuda_cflags += -DARCH_ARM
_local_cuda_cflags += -DDEBUG_MODE
_local_cuda_cflags += -I$(TMP_NDK)/platforms/android-18/arch-arm/usr/include
_local_cuda_cflags += -I$(ANDROID_CUDA_PATH)/targets/$(CUDA_EABI)/include

# We override the compiler location because the current version of NVCC & friends
# require version 4.6 of the compiler.
_local_cuda_cflags += -I$(TMP_NDK)sources/cxx-stl/gnu-libstdc++/4.6/include
_local_cuda_cflags += -I$(TMP_NDK)sources/cxx-stl/gnu-libstdc++/4.6/libs/armeabi-v7a/include

# Let the user override flags above using LOCAL_CFLAGS
#_save_local_cflags := $(LOCAL_CFLAGS)
LOCAL_CFLAGS := $(_local_cuda_cflags)
#LOCAL_CFLAGS += $(_save_local_cflags)


NVCC_CC := $(TMP_GPP)/bin/arm-linux-androideabi-g++

include $(NVIDIA_BASE)

# Clear out "-mno-unaligned-access" - it will cause nvcc to complain
LOCAL_CFLAGS := $(filter-out -mno-unaligned-access,$(LOCAL_CFLAGS))

# Clear out other LOCAL_CFLAGS as needed
LOCAL_CFLAGS_arm :=
LOCAL_CFLAGS_32 :=

include $(BUILD_SYSTEM)/base_rules.mk

cuda_sources := $(filter %.cu,$(LOCAL_SRC_FILES))
cuda_objects := $(addprefix $(intermediates)/,$(cuda_sources:.cu=.o))

ALL_C_CPP_ETC_OBJECTS += $(cuda_objects)

$(cuda_objects): PRIVATE_CC         := $(LOCAL_CC) -ccbin $(NVCC_CC)
$(cuda_objects): PRIVATE_CFLAGS     := $(LOCAL_CFLAGS)
$(cuda_objects): PRIVATE_C_INCLUDES := $(LOCAL_C_INCLUDES)
$(cuda_objects): PRIVATE_MODULE     := $(LOCAL_MODULE)

ifneq ($(strip $(cuda_objects)),)
$(cuda_objects): $(intermediates)/%.o: $(LOCAL_PATH)/%.cu \
    $(LOCAL_ADDITIONAL_DEPENDENCIES) \
	| $(LOCAL_CC) $(NVCC_CC)
	@echo "target CUDA: $(PRIVATE_MODULE) <= $<"
	@echo "C includes: $(PRIVATE_C_INCLUDES) end"
	@mkdir -p $(dir $@)
	$(hide) $(PRIVATE_CC) \
	    $(addprefix -I , $(PRIVATE_C_INCLUDES)) \
	    $(PRIVATE_CFLAGS) \
	    -o $@ --compiler-options -MD,-MF,$(patsubst %.o,%.d,$@) -c $<
	$(transform-d-to-p)
-include $(cuda_objects:%.o=%.P)
endif

$(LOCAL_BUILT_MODULE): PRIVATE_CC := $(LOCAL_CC) -ccbin $(NVCC_CC) -lib
$(LOCAL_BUILT_MODULE): $(cuda_objects) \
	| $(LOCAL_CC) $(NVCC_CC)
	@mkdir -p $(dir $@)
	@rm -f $@
	@echo "target CUDA StaticLib: $(PRIVATE_MODULE) ($@)"
	$(hide) $(call split-long-arguments,$(PRIVATE_CC) -o $@,$(filter %.o, $^))
	touch $(dir $@)export_includes
