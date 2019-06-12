#
# Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
#

ifeq ($(NVIDIA_CLEARED),false)
$(error $(LOCAL_PATH): NVIDIA variables not cleared)
endif
NVIDIA_CLEARED := false

# Protect against an empty LOCAL_PATH
ifeq ($(LOCAL_PATH),)
$(error $(NVIDIA_MAKEFILE): empty LOCAL_PATH is not allowed))
endif

# Protect against absolute paths in LOCAL_SRC_FILES
ifneq ($(filter /%, $(dir $(LOCAL_SRC_FILES))),)
$(error $(LOCAL_PATH): absolute paths are not allowed in LOCAL_SRC_FILES)
endif

# Protect against ../ in paths in LOCAL_SRC_FILES
ifneq ($(findstring ../, $(dir $(LOCAL_SRC_FILES))),)
$(error $(LOCAL_PATH): ../ in path is not allowed for LOCAL_SRC_FILES)
endif

ifeq ($(LOCAL_IS_HOST_MODULE),true)
#
# Nvidia host code debug flag fixup
#
# Default debug flags are set in defaults.mk based on $(TARGET_BUILD_TYPE),
# which are incorrect for host code if $(HOST_BUILD_TYPE) is different. But
# some components expect to be able to override the debug settings from
# so it can't be removed from defaults.mk.
#
# check if fixup is needed
ifneq ($(TARGET_BUILD_TYPE),$(HOST_BUILD_TYPE))

# component uses own set of debug flags -> don't touch them
ifneq ($(LOCAL_NVIDIA_OVERRIDE_HOST_DEBUG_FLAGS),1)

# NOTE: this conditional needs to be kept in sync with the one in defaults.mk!
ifeq ($(HOST_BUILD_TYPE),debug)

# TARGET_BUILD_TYPE == release
LOCAL_CFLAGS += -UNV_DEBUG -DNV_DEBUG=1
# TODO: fix source that relies on these
LOCAL_CFLAGS += -UDEBUG -DDEBUG
LOCAL_CFLAGS += -U_DEBUG -D_DEBUG

else

# TARGET_BUILD_TYPE == debug
LOCAL_CFLAGS += -UNV_DEBUG -DNV_DEBUG=0
LOCAL_CFLAGS += -UDEBUG
LOCAL_CFLAGS += -U_DEBUG

endif
endif

ifeq ($(HOST_BUILD_TYPE),debug)
# disable all optimizations and enable gdb debugging extensions
LOCAL_CFLAGS += -O0 -ggdb
endif

endif
endif

# output directory for generated files

ifneq ($(findstring $(LOCAL_MODULE_CLASS),EXECUTABLES STATIC_LIBRARIES SHARED_LIBRARIES),)

intermediates := $(local-intermediates-dir)

# shader rules

# LOCAL_NVIDIA_SHADERS is relative to LOCAL_PATH
# LOCAL_NVIDIA_GEN_SHADERS is relative to intermediates

ifneq ($(strip $(LOCAL_NVIDIA_SHADERS) $(LOCAL_NVIDIA_GEN_SHADERS)),)

# Cg and GLSL shader binaries (.cghex) and source (.xxxh)

$(foreach shadertype,glslv glslf cgv cgf,\
	$(eval $(call shader-rule,$(shadertype),\
		$(LOCAL_NVIDIA_SHADERS),\
		$(LOCAL_NVIDIA_GEN_SHADERS))))

$(ALL_SHADERS_COMPILE_glslv): PRIVATE_CGOPTS := -profile ar20vp -ogles $(LOCAL_NVIDIA_CGOPTS) $(LOCAL_NVIDIA_CGVERTOPTS)
$(ALL_SHADERS_COMPILE_glslf): PRIVATE_CGOPTS := -profile ar20fp -ogles $(LOCAL_NVIDIA_CGOPTS) $(LOCAL_NVIDIA_CGFRAGOPTS)
$(ALL_SHADERS_COMPILE_cgv): PRIVATE_CGOPTS := -profile ar20vp $(LOCAL_NVIDIA_CGOPTS) $(LOCAL_NVIDIA_CGVERTOPTS)
$(ALL_SHADERS_COMPILE_cgf): PRIVATE_CGOPTS := -profile ar20fp $(LOCAL_NVIDIA_CGOPTS) $(LOCAL_NVIDIA_CGFRAGOPTS)

$(ALL_SHADERS_COMPILE_glslv) $(ALL_SHADERS_COMPILE_glslf) $(ALL_SHADERS_COMPILE_cgv) $(ALL_SHADERS_COMPILE_cgf): $(NVIDIA_CGC)
$(ALL_SHADERS_glslv) $(ALL_SHADERS_glslf) $(ALL_SHADERS_cgv) $(ALL_SHADERS_cgf): $(NVIDIA_SHADERFIX)

# Ar20 assembly to header (.h)

GEN_AR20FRG := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.ar20frg,%.h,$(filter %.ar20frg,$(LOCAL_NVIDIA_SHADERS))))
$(GEN_AR20FRG): $(intermediates)/shaders/%.h : $(LOCAL_PATH)/%.ar20frg
	$(transform-ar20asm-to-h)
$(GEN_AR20FRG): $(NVIDIA_AR20ASM)

# Common dependencies and declarations

ALL_GENERATED_FILES := $(foreach shadertype,glslv glslf cgv cgf,\
		           $(ALL_SHADERS_$(shadertype)) \
		           $(ALL_SHADERS_NOFIX_$(shadertype)) \
			   $(ALL_SHADERSRC_$(shadertype))) $(GEN_AR20FRG)

LOCAL_GENERATED_SOURCES += $(ALL_GENERATED_FILES)
LOCAL_C_INCLUDES += $(sort $(dir $(ALL_GENERATED_FILES)))

endif
endif

# NVIDIA build targets

ifneq ($(LOCAL_MODULE),)
NVIDIA_TARGET_NAME := $(LOCAL_MODULE)
else ifneq ($(LOCAL_PACKAGE_NAME),)
NVIDIA_TARGET_NAME := $(LOCAL_PACKAGE_NAME)
else ifneq ($(LOCAL_PREBUILT_EXECUTABLES),)
NVIDIA_TARGET_NAME := $(LOCAL_PREBUILT_EXECUTABLES)
else
$(error $(LOCAL_PATH): One of LOCAL_MODULE, LOCAL_PACKAGE_NAME or LOCAL_PREBUILT_EXECUTABLES must be defined in the Android makefile!)
endif

# Add to nvidia module list
ALL_NVIDIA_MODULES += $(NVIDIA_TARGET_NAME)

# Add dependency to Android.mk
ifeq ($(filter $(LOCAL_PATH)/%,$(NVIDIA_MAKEFILE)),)
$(warning $(NVIDIA_MAKEFILE) not under $(LOCAL_PATH) for module $(NVIDIA_TARGET_NAME))
else
LOCAL_ADDITIONAL_DEPENDENCIES += $(NVIDIA_MAKEFILE)
endif

# If modules are in vendor/nvidia, but not 3rdparty, then they should be ours
ifneq ($(findstring vendor/nvidia,$(LOCAL_PATH)),)
ifeq ($(findstring 3rdparty,$(LOCAL_PATH)),)
LOCAL_PROPRIETARY_MODULE := true

ifeq ($(LOCAL_MODULE_OWNER),)
LOCAL_MODULE_OWNER := nvidia
endif

# For apps without a set path, force them back to /system/app, since dexopt fails in /vendor/app
ifneq ($(filter APPS JAVA_LIBRARIES,$(LOCAL_MODULE_CLASS)),)
  ifeq ($(LOCAL_MODULE_PATH),)
    ifeq (true,$(LOCAL_PRIVILEGED_MODULE))
      LOCAL_MODULE_PATH := $(TARGET_OUT_$(LOCAL_MODULE_CLASS)_PRIVILEGED)
    else
      LOCAL_MODULE_PATH := $(TARGET_OUT_$(LOCAL_MODULE_CLASS))
    endif
  endif
endif
endif
endif

# Add to nvidia goals
nvidia-clean: clean-$(NVIDIA_TARGET_NAME)

ifneq ($(findstring nvidia_tests,$(LOCAL_MODULE_TAGS)),)
nvidia-tests: $(NVIDIA_TARGET_NAME)
ifneq ($(filter nvidia-tests,$(MAKECMDGOALS)),)
# If we're explicitly building nvidia-tests, install the tests.
ALL_NVIDIA_TESTS += $(NVIDIA_TARGET_NAME)
ifeq ($(LOCAL_MODULE_CLASS),JAVA_LIBRARIES)
# We want Nvidia java test libraries to be installed into same
# location as normal java libraries. Android build system would in
# default place them in location pointed by
# TARGET_OUT_DATA_JAVA_LIBRARIES (since LOCAL_MODULE_TAGS indicates
# them to be 'tests' components).
LOCAL_MODULE_PATH := $(TARGET_OUT_JAVA_LIBRARIES)
endif
endif

ifneq ($(filter nvidia-tests-automation,$(MAKECMDGOALS)),)
# If we're explicitly building nvidia-tests-automation, redirect the tests.
ALL_NVIDIA_TESTS += $(NVIDIA_TARGET_NAME)
ifeq ($(LOCAL_MODULE_CLASS),EXECUTABLES)
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/nvidia_tests/system/bin
else ifeq ($(LOCAL_MODULE_CLASS),SHARED_LIBRARIES)
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/nvidia_tests/system/lib
else ifeq ($(LOCAL_MODULE_CLASS),JAVA_LIBRARIES)
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/nvidia_tests/system/framework
else ifneq ($(LOCAL_MODULE_PATH),)
    ifeq ($(findstring nvidia_tests,$(LOCAL_MODULE_PATH)),)
      # Insert nvidia_tests in the installation path.
      LOCAL_MODULE_PATH := $(subst $(PRODUCT_OUT),$(PRODUCT_OUT)/nvidia_tests,$(LOCAL_MODULE_PATH))
    endif
else
# Specify default install location for everything else.
LOCAL_MODULE_PATH := $(PRODUCT_OUT)/nvidia_tests
endif # ifeq ($(LOCAL_MODULE_CLASS),EXECUTABLES)
endif # ifneq ($(filter nvidia-tests-automation,$(MAKECMDGOALS)),)

LOCAL_MODULE_TAGS := $(filter-out nvidia_tests,$(LOCAL_MODULE_TAGS)) tests
else # not nvidia-test component
nvidia-modules: $(NVIDIA_TARGET_NAME)
endif # ifneq ($(findstring nvidia_tests,$(LOCAL_MODULE_TAGS)),)

nvidia-tests-automation: nvidia-tests

NVIDIA_TARGET_NAME :=

# For GCC version > 4.6, we should add "-mno-unaligned-access" compiling flag for Nvidia modules
# (except for host modules, because GCC does not support this compiling flag for x86)
ifneq ($(TARGET_GCC_VERSION),4.6)
ifneq ($(LOCAL_IS_HOST_MODULE),true)
LOCAL_CFLAGS += -mno-unaligned-access
endif
endif
