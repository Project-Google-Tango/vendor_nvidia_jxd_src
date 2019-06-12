ifneq ($(filter-out SHARED_LIBRARIES EXECUTABLES,$(LOCAL_MODULE_CLASS)),)
$(error The integration layer for the nvmake build system supports only shared libraries and executables)
endif

include $(NVIDIA_BASE)
# TODO Enable coverage build

# Set to 1 to do nvmake builds in the unix-build chroot
NV_USE_UNIX_BUILD ?= 0

ifeq ($(LOCAL_NVIDIA_NVMAKE_OVERRIDE_BUILD_TYPE),)
  NVIDIA_NVMAKE_BUILD_TYPE := $(TARGET_BUILD_TYPE)
else
  NVIDIA_NVMAKE_BUILD_TYPE := $(LOCAL_NVIDIA_NVMAKE_OVERRIDE_BUILD_TYPE)
endif

ifeq ($(NV_MOBILE_DGPU),1)
  # Using this library path causes build failures on Ubuntu 12.04, but
  # dGPU builds require it.
  NVIDIA_NVMAKE_LIBRARY_PATH := $(P4ROOT)/sw/mobile/tools/linux/android/nvmake/unix-build/lib
else
  NVIDIA_NVMAKE_LIBRARY_PATH := $(LD_LIBRARY_PATH)
endif

ifeq ($(LOCAL_NVIDIA_NVMAKE_OVERRIDE_MODULE_NAME),)
  NVIDIA_NVMAKE_MODULE_NAME := $(LOCAL_MODULE)
else
  NVIDIA_NVMAKE_MODULE_NAME := $(LOCAL_NVIDIA_NVMAKE_OVERRIDE_MODULE_NAME)
endif

ifeq ($(LOCAL_NVIDIA_NVMAKE_OVERRIDE_TOP),)
  ifeq ($(LOCAL_NVIDIA_NVMAKE_TREE),drv)
    NVIDIA_NVMAKE_TOP := $(NV_GPUDRV_SOURCE)
  else
    NVIDIA_NVMAKE_TOP := $(TEGRA_TOP)/gpu/$(LOCAL_NVIDIA_NVMAKE_TREE)
  endif
else
  NVIDIA_NVMAKE_TOP := $(LOCAL_NVIDIA_NVMAKE_OVERRIDE_TOP)
endif

ifeq ($(LOCAL_NVIDIA_NVMAKE_OVERRIDE_TARGET_ABI),)
  NVIDIA_NVMAKE_TARGET_ABI := androideabi
else
  NVIDIA_NVMAKE_TARGET_ABI := $(LOCAL_NVIDIA_NVMAKE_OVERRIDE_TARGET_ABI)
endif

ifeq ($(LOCAL_NVIDIA_NVMAKE_OVERRIDE_TARGET_OS),)
  NVIDIA_NVMAKE_TARGET_OS := Android
else
  NVIDIA_NVMAKE_TARGET_OS := $(LOCAL_NVIDIA_NVMAKE_OVERRIDE_TARGET_OS)
endif

# Android builds set NV_INTERNAL_PROFILE in internal builds, and nothing
# on external builds. Convert this to nvmake convention.
ifeq ($(NV_INTERNAL_PROFILE),1)
  NVIDIA_NVMAKE_PROFILE :=
else
  NVIDIA_NVMAKE_PROFILE := NV_EXTERNAL_PROFILE=1
endif

NVIDIA_NVMAKE_UNIX_BUILD_COMMAND := \
  unix-build \
  --no-devrel \
  --extra $(ANDROID_BUILD_TOP) \
  --extra $(P4ROOT)/sw/tools \
  --tools $(P4ROOT)/sw/tools \
  --source $(NVIDIA_NVMAKE_TOP)

ifeq ($(NVIDIA_NVMAKE_TARGET_OS),Android)
  # Android uses a special chroot, compatible with the 64-bit binaries of the Google toolchain,
  # as well as a copy of this toolchain.
  NVIDIA_NVMAKE_UNIX_BUILD_COMMAND += \
    --extra-with-bind-point $(P4ROOT)/sw/mobile/tools/linux/android/nvmake/unix-build64/lib /lib \
    --extra-with-bind-point $(P4ROOT)/sw/mobile/tools/linux/android/nvmake/unix-build64/lib32 /lib32 \
    --extra-with-bind-point $(P4ROOT)/sw/mobile/tools/linux/android/nvmake/unix-build64/lib64 /lib64 \
    --extra $(P4ROOT)/sw/mobile/tools/linux/android/nvmake
endif

NVIDIA_NVMAKE_MODULE_PRIVATE_PATH := $(LOCAL_NVIDIA_NVMAKE_OVERRIDE_MODULE_PRIVATE_PATH)

NVIDIA_NVMAKE_MODULE := \
    $(NVIDIA_NVMAKE_TOP)/$(LOCAL_NVIDIA_NVMAKE_BUILD_DIR)/_out/$(NVIDIA_NVMAKE_TARGET_OS)_ARMv7_$(NVIDIA_NVMAKE_TARGET_ABI)_$(NVIDIA_NVMAKE_BUILD_TYPE)/$(NVIDIA_NVMAKE_MODULE_PRIVATE_PATH)/$(NVIDIA_NVMAKE_MODULE_NAME)$(LOCAL_MODULE_SUFFIX)

ifneq ($(strip $(SHOW_COMMANDS)),)
  NVIDIA_NVMAKE_VERBOSE := NV_VERBOSE=1
else
  NVIDIA_NVMAKE_VERBOSE := -s
endif

#
# Call into the nvmake build system to build the module
#
# Add NVUB_SUPPORTS_TXXX=1 to temporarily enable a chip
#

$(NVIDIA_NVMAKE_MODULE) $(LOCAL_MODULE)_nvmakeclean: NVIDIA_NVMAKE_COMMON_BUILD_PARAMS := \
    TEGRA_TOP=$(TEGRA_TOP) \
    ANDROID_BUILD_TOP=$(ANDROID_BUILD_TOP) \
    OUT=$(OUT) \
    NV_SOURCE=$(NVIDIA_NVMAKE_TOP) \
    NV_TOOLS=$(P4ROOT)/sw/tools \
    NV_HOST_OS=Linux \
    NV_HOST_ARCH=x86 \
    NV_TARGET_OS=$(NVIDIA_NVMAKE_TARGET_OS) \
    NV_TARGET_ARCH=ARMv7 \
    NV_BUILD_TYPE=$(NVIDIA_NVMAKE_BUILD_TYPE) \
    $(NVIDIA_NVMAKE_PROFILE) \
    NV_MANGLE_GLSI=0 \
    NV_COVERAGE_ENABLED=$(NVIDIA_COVERAGE_ENABLED) \
    TARGET_TOOLS_PREFIX=$(abspath $(TARGET_TOOLS_PREFIX)) \
    TARGET_C_INCLUDES="$(foreach inc,external/stlport/stlport $(TARGET_C_INCLUDES) bionic,$(abspath $(inc)))" \
    TARGET_OUT_INTERMEDIATE_LIBRARIES=$(abspath $(TARGET_OUT_INTERMEDIATE_LIBRARIES)) \
    TARGET_LIBGCC=$(TARGET_LIBGCC) \
    $(NVIDIA_NVMAKE_VERBOSE) \
    $(LOCAL_NVIDIA_NVMAKE_ARGS)

ifeq ($(NV_USE_UNIX_BUILD),1)
  $(NVIDIA_NVMAKE_MODULE) $(LOCAL_MODULE)_nvmakeclean: NVIDIA_NVMAKE_COMMAND := \
    $(NVIDIA_NVMAKE_UNIX_BUILD_COMMAND) \
    --newdir $(NVIDIA_NVMAKE_TOP)/$(LOCAL_NVIDIA_NVMAKE_BUILD_DIR) \
    nvmake
else
  $(NVIDIA_NVMAKE_MODULE) $(LOCAL_MODULE)_nvmakeclean: NVIDIA_NVMAKE_COMMAND := \
    $(MAKE) \
    MAKE=$(shell which $(MAKE)) \
    LD_LIBRARY_PATH=$(NVIDIA_NVMAKE_LIBRARY_PATH) \
    NV_UNIX_BUILD_CHROOT=$(P4ROOT)/sw/tools/unix/hosts/Linux-x86/unix-build \
    -C $(NVIDIA_NVMAKE_TOP)/$(LOCAL_NVIDIA_NVMAKE_BUILD_DIR) \
    -f makefile.nvmk
endif

$(NVIDIA_NVMAKE_MODULE) $(LOCAL_MODULE)_nvmakeclean: NVIDIA_NVMAKE_RM_MODULE_MAKE := \
    $(MAKE) \
    MAKE=$(shell which $(MAKE)) \
    PATH=$(ARM_EABI_TOOLCHAIN):/usr/bin:$(PATH) \
    CROSS_COMPILE=arm-eabi- \
    ARCH=arm \
    SYSOUT=$(ANDROID_BUILD_TOP)/$(TARGET_OUT_INTERMEDIATES)/KERNEL \
    SYSSRC=$(ANDROID_BUILD_TOP)/kernel \
    CC=$(ARM_EABI_TOOLCHAIN)/arm-eabi-gcc \
    LD=$(ARM_EABI_TOOLCHAIN)/arm-eabi-ld \
    NV_MOBILE_DGPU=$(NV_MOBILE_DGPU) \
    -f Makefile

ifeq ($(LOCAL_NVIDIA_NVMAKE_BUILD_DIR), drivers/resman)
  $(NVIDIA_NVMAKE_MODULE): NVIDIA_NVMAKE_POST_BUILD_COMMAND := \
    cd $(dir $(NVIDIA_NVMAKE_MODULE)) && \
    $(NVIDIA_NVMAKE_RM_MODULE_MAKE) clean && \
    $(NVIDIA_NVMAKE_RM_MODULE_MAKE) nv-linux.o
else
  $(NVIDIA_NVMAKE_MODULE): NVIDIA_NVMAKE_POST_BUILD_COMMAND :=
endif

# We always link nvmake components against these few libraries.
# LOCAL_SHARED_LIBRARIES will enforce the install requirement, but
# LOCAL_ADDITIONAL_DEPENDENCIES will enforce that they are built before nvmake runs
LOCAL_SHARED_LIBRARIES += libc libdl libm libstdc++ libz
# Ensure libgcov_null.so is built if needed.
ifeq ($(TARGET_BUILD_TYPE),debug)
  ifneq ($(NVIDIA_COVERAGE_ENABLED),)
    ifneq ($(LOCAL_NVIDIA_NO_COVERAGE),true)
      ifeq ($(LOCAL_NVIDIA_NULL_COVERAGE),true)
        LOCAL_SHARED_LIBRARIES += libgcov_null
      endif
    endif
  endif
endif

LOCAL_ADDITIONAL_DEPENDENCIES += \
	$(foreach l,$(LOCAL_SHARED_LIBRARIES),$(TARGET_OUT_INTERMEDIATE_LIBRARIES)/$(l).so)

# This target needs to be forced, nvmake will do its own dependency checking
$(NVIDIA_NVMAKE_MODULE): $(LOCAL_ADDITIONAL_DEPENDENCIES) FORCE
	@echo "Build with nvmake: $(PRIVATE_MODULE) ($@)"
	+$(hide) $(NVIDIA_NVMAKE_COMMAND) $(NVIDIA_NVMAKE_COMMON_BUILD_PARAMS) MAKEFLAGS="$(MAKEFLAGS)"
	+$(hide) $(NVIDIA_NVMAKE_POST_BUILD_COMMAND)

$(LOCAL_MODULE)_nvmakeclean:
	@echo "Clean nvmake build files: $(PRIVATE_MODULE)"
	+$(hide) $(NVIDIA_NVMAKE_COMMAND) $(NVIDIA_NVMAKE_COMMON_BUILD_PARAMS) MAKEFLAGS="$(MAKEFLAGS)" clobber

.PHONY: $(LOCAL_MODULE)_nvmakeclean

#
# Bring module from the nvmake build output, and apply the usual
# processing for shared library or executable.
# Also make the module's clean target descend into nvmake.
#

include $(BUILD_SYSTEM)/dynamic_binary.mk

$(linked_module): $(NVIDIA_NVMAKE_MODULE) | $(ACP)
	@echo "Copy from nvmake output: $(PRIVATE_MODULE) ($@)"
	$(copy-file-to-target)

ifeq ($(LOCAL_NVIDIA_NVMAKE_BUILD_DIR), drivers/resman)

$(strip_output): NVIDIA_NVMAKE_RM_KERNEL_INTERFACE_PATH := \
    $(NVIDIA_NVMAKE_TOP)/$(LOCAL_NVIDIA_NVMAKE_BUILD_DIR)/_out/$(NVIDIA_NVMAKE_TARGET_OS)_ARMv7_$(NVIDIA_NVMAKE_TARGET_ABI)_$(NVIDIA_NVMAKE_BUILD_TYPE)/$(NVIDIA_NVMAKE_MODULE_PRIVATE_PATH)

$(strip_output): NVIDIA_NVMAKE_RM_STRIP := \
    $(ARM_EABI_TOOLCHAIN)/arm-eabi-strip -g

NVIDIA_NVMAKE_RM_INSTALLER_OBJ_FILES := \
    $(TARGET_OUT_VENDOR)/nvidia/dgpu/nv-kernel.o \
    $(TARGET_OUT_VENDOR)/nvidia/dgpu/nv-linux.o \
    $(TARGET_OUT_VENDOR)/nvidia/dgpu/nvidia.mod.o

$(strip_output): $(NVIDIA_NVMAKE_RM_INSTALLER_OBJ_FILES) \
    $(TARGET_OUT_VENDOR)/nvidia/dgpu/module-common.lds \
    $(TARGET_OUT_EXECUTABLES)/makedevices.sh \
    $(TARGET_OUT_EXECUTABLES)/nv-dgpu-install.sh \
    $(TARGET_OUT_EXECUTABLES)/arm-android-eabi-ld

$(strip_output): $(linked_module) | $(ACP)
	@echo "target Stripped: $(PRIVATE_MODULE) ($@)"
	$(copy-file-to-target)
	+$(hide) $(ACP) -fp $@ $@.sym
	+$(hide) $(NVIDIA_NVMAKE_RM_STRIP) $@

# Define a rule to copy and strip rm obj file.  For use via $(eval).
# $(1): file to copy
define copy-and-strip-rm-obj-file
$(1): $(NVIDIA_NVMAKE_MODULE) | $(ACP)
	@echo "target Stripped: $(notdir $(1)) ($(1))"
	@mkdir -p $$(TARGET_OUT_VENDOR)/nvidia/dgpu
	+$(hide) $(ACP) -fp $$(NVIDIA_NVMAKE_RM_KERNEL_INTERFACE_PATH)/$(notdir $(1)) $(1)
	+$(hide) $$(NVIDIA_NVMAKE_RM_STRIP) $(1)
endef

$(foreach file,$(NVIDIA_NVMAKE_RM_INSTALLER_OBJ_FILES), \
    $(eval $(call copy-and-strip-rm-obj-file,$(file))))

$(TARGET_OUT_VENDOR)/nvidia/dgpu/module-common.lds: $(TOP)/kernel/scripts/module-common.lds | $(ACP)
	@echo "Copy $< => $@"
	+$(hide) $(copy-file-to-target)

$(TARGET_OUT_EXECUTABLES)/makedevices.sh: $(NVIDIA_NVMAKE_TOP)/$(LOCAL_NVIDIA_NVMAKE_BUILD_DIR)/arch/nvalloc/unix/Linux/makedevices.sh | $(ACP)
	@echo "Copy $< => $@"
	+$(hide) $(copy-file-to-target)

$(TARGET_OUT_EXECUTABLES)/arm-android-eabi-ld: $(TEGRA_TOP)/3rdparty/binutils/bin/arm-android-eabi-ld | $(ACP)
	@echo "Copy $< => $@"
	+$(hide) $(copy-file-to-target)

$(TARGET_OUT_EXECUTABLES)/nv-dgpu-install.sh: $(TEGRA_TOP)/core/drivers/nvrm/dgpu/nv-dgpu-install.sh | $(ACP)
	@echo "Copy $< => $@"
	+$(hide) $(copy-file-to-target)

endif

$(cleantarget):: $(LOCAL_MODULE)_nvmakeclean

NVIDIA_NVMAKE_BUILD_TYPE :=
NVIDIA_NVMAKE_TOP :=
NVIDIA_NVMAKE_LIBRARY_PATH :=
NVIDIA_NVMAKE_MODULE :=
NVIDIA_NVMAKE_MODULE_NAME :=
NVIDIA_NVMAKE_PROFILE :=
NVIDIA_NVMAKE_VERBOSE :=
NVIDIA_NVMAKE_TARGET_ABI :
NVIDIA_NVMAKE_TARGET_OS :=
NVIDIA_NVMAKE_MODULE_PRIVATE_PATH :=
NVIDIA_NVMAKE_RM_KERNEL_INTERFACE_PATH :=
NVIDIA_NVMAKE_RM_INSTALLER_OBJ_FILES :=
NVIDIA_NVMAKE_UNIX_BUILD_COMMAND :=
