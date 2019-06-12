#
# Copyright (c) 2010-2013, NVIDIA CORPORATION.  All rights reserved.
#
# Global build system definitions go here
#

# Inherit build shell from Android if applicable
ifdef ANDROID_BUILD_SHELL
SHELL := $(ANDROID_BUILD_SHELL)
endif

ifndef NV_TARGET_BOOTLOADER_PINMUX
NV_TARGET_BOOTLOADER_PINMUX := kernel
endif

ifndef TEGRA_TOP
TEGRA_TOP := vendor/nvidia/tegra
endif

NVIDIA_BUILD_ROOT          := vendor/nvidia/build

#Bug 1266062: Temporary switch to use new ODM repo sync location path
#This needs to be removed later
ifndef NV_BUILD_WAR_1266062
NV_BUILD_WAR_1266062 := 1
endif

# Bug 1251947: Choose which location to build NVogtest
# This needs to be removed later
ifndef NV_BUILD_WAR_1251947
NV_BUILD_WAR_1251947 := apps-graphics
endif

ifndef NV_GPUDRV_SOURCE
NV_GPUDRV_SOURCE := $(TEGRA_TOP)/gpu/drv
endif

include vendor/nvidia/build/detectversion.mk

# links to build system files

NVIDIA_CUDA_STATIC_LIBRARY := $(NVIDIA_BUILD_ROOT)/cuda_static_library.mk
NVIDIA_BASE                := $(NVIDIA_BUILD_ROOT)/base.mk
NVIDIA_DEBUG               := $(NVIDIA_BUILD_ROOT)/debug.mk
NVIDIA_DEFAULTS            := $(NVIDIA_BUILD_ROOT)/defaults.mk
NVIDIA_STATIC_LIBRARY      := $(NVIDIA_BUILD_ROOT)/static_library.mk
NVIDIA_STATIC_AVP_LIBRARY  := $(NVIDIA_BUILD_ROOT)/static_avp_library.mk
NVIDIA_SHARED_LIBRARY      := $(NVIDIA_BUILD_ROOT)/shared_library.mk
NVIDIA_EXECUTABLE          := $(NVIDIA_BUILD_ROOT)/executable.mk
NVIDIA_NVMAKE_BASE         := $(NVIDIA_BUILD_ROOT)/nvmake_base.mk
NVIDIA_NVMAKE_SHARED_LIBRARY := $(NVIDIA_BUILD_ROOT)/nvmake_shared_library.mk
NVIDIA_NVMAKE_EXECUTABLE   := $(NVIDIA_BUILD_ROOT)/nvmake_executable.mk
NVIDIA_STATIC_AVP_EXECUTABLE := $(NVIDIA_BUILD_ROOT)/static_avp_executable.mk
NVIDIA_STATIC_EXECUTABLE := $(NVIDIA_BUILD_ROOT)/static_executable.mk
NVIDIA_STATIC_AND_SHARED_LIBRARY := $(NVIDIA_BUILD_ROOT)/static_and_shared_library.mk
NVIDIA_HOST_STATIC_LIBRARY := $(NVIDIA_BUILD_ROOT)/host_static_library.mk
NVIDIA_HOST_SHARED_LIBRARY := $(NVIDIA_BUILD_ROOT)/host_shared_library.mk
NVIDIA_HOST_EXECUTABLE     := $(NVIDIA_BUILD_ROOT)/host_executable.mk
NVIDIA_JAVA_LIBRARY        := $(NVIDIA_BUILD_ROOT)/java_library.mk
NVIDIA_STATIC_JAVA_LIBRARY := $(NVIDIA_BUILD_ROOT)/static_java_library.mk
NVIDIA_PACKAGE             := $(NVIDIA_BUILD_ROOT)/package.mk
NVIDIA_COVERAGE            := $(NVIDIA_BUILD_ROOT)/coverage.mk
NVIDIA_PREBUILT            := $(NVIDIA_BUILD_ROOT)/prebuilt.mk
NVIDIA_MULTI_PREBUILT      := $(NVIDIA_BUILD_ROOT)/multi_prebuilt.mk
NVIDIA_PREBUILT_NOTICE     := $(NVIDIA_BUILD_ROOT)/nv_prebuilt_notice_files.mk
NVIDIA_HOST_PREBUILT       := $(NVIDIA_BUILD_ROOT)/host_prebuilt.mk
NVIDIA_WARNINGS            := $(NVIDIA_BUILD_ROOT)/warnings.mk
NVIDIA_GENERATED_HEADER    := $(NVIDIA_BUILD_ROOT)/generated_headers.mk
NVIDIA_UBM_DEFAULTS        := $(NVIDIA_BUILD_ROOT)/ubm_defaults.mk

# compiler

NVIDIA_AR20ASM             := $(TEGRA_TOP)/cg/Cg/$(HOST_OS)/ar20asm
NVIDIA_CGC                 := $(HOST_OUT_EXECUTABLES)/cgc
NVIDIA_CGC_PROFILE         := glest114
NVIDIA_SHADERFIX           := $(HOST_OUT_EXECUTABLES)/shaderfix
NVIDIA_AR20SHADERLAYOUT    := $(HOST_OUT_EXECUTABLES)/ar20shaderlayout

# tools

NVIDIA_GETEXPORTS          := $(NVIDIA_BUILD_ROOT)/getexports.py
NVIDIA_HEXIFY              := $(NVIDIA_BUILD_ROOT)/hexify.py

# global vars
ALL_NVIDIA_MODULES :=
ALL_NVIDIA_TESTS :=
ifneq ($(TEGRA_TOP),hardware/tegra)
NVIDIA_APICHECK := 1
endif

# rule generation to be used via $(call)

define transform-shader-to-cgbin
@echo "Compiling shader $@ from $<"
@mkdir -p $(@D)
$(hide) cat $< | $(NVIDIA_CGC) -quiet $(PRIVATE_CGOPTS) -o $(basename $@).cgbin
endef

define transform-cgbin-to-cghex
@echo "Generating shader binary $@ from $<"
@mkdir -p $(@D)
$(hide) $(NVIDIA_SHADERFIX) -o $(basename $@).ar20bin $(basename $@).cgbin
$(hide) $(NVIDIA_HEXIFY) $(basename $@).ar20bin $@
endef

define transform-cgbin-to-h
@echo "Generating non-shaderfixed binary $@ from $<"
@mkdir -p $(@D)
$(hide) $(NVIDIA_HEXIFY) $(basename $@).cgbin $@
endef

define transform-shader-to-string
@echo "Generating shader source $@ from $<"
@mkdir -p $(@D)
$(hide) cat $< | sed -e 's|^.*$$|"&\\n"|' > $@
endef

define transform-ar20asm-to-h
@echo "Generating shader $@ from $<"
@mkdir -p $(@D)
$(hide) LD_LIBRARY_PATH=$(TEGRA_TOP)/cg/Cg/$(HOST_OS) $(NVIDIA_AR20ASM) $< $(basename $@).ar20bin
$(hide) $(NVIDIA_HEXIFY) $(basename $@).ar20bin $@
endef

define shader-rule
# shaders and shader source to output
SHADERS_COMPILE_$(1) := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.$(1),%.cgbin,$(filter %.$(1),$(2))))
GEN_SHADERS_COMPILE_$(1) := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.$(1),%.cgbin,$(filter %.$(1),$(3))))
SHADERS_$(1) := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.$(1),%.cghex,$(filter %.$(1),$(2))))
GEN_SHADERS_$(1) := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.$(1),%.cghex,$(filter %.$(1),$(3))))
SHADERS_NOFIX_$(1) := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.$(1),%.h,$(filter %.$(1),$(2))))
GEN_SHADERS_NOFIX_$(1) := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.$(1),%.h,$(filter %.$(1),$(3))))
SHADERSRC_$(1) := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.$(1),%.$(1)h,$(filter %.$(1),$(2))))
GEN_SHADERSRC_$(1) := $(addprefix $(intermediates)/shaders/, \
	$(patsubst %.$(1),%.$(1)h,$(filter %.$(1),$(3))))

# create lists to "output"
ALL_SHADERS_COMPILE_$(1) := $$(SHADERS_COMPILE_$(1)) $$(GEN_SHADERS_COMPILE_$(1))
ALL_SHADERS_$(1) := $$(SHADERS_$(1)) $$(GEN_SHADERS_$(1))
ALL_SHADERS_NOFIX_$(1) := $$(SHADERS_NOFIX_$(1)) $$(GEN_SHADERS_NOFIX_$(1))
ALL_SHADERSRC_$(1) := $$(SHADERSRC_$(1)) $$(GEN_SHADERSRC_$(1))

# rules for building the shaders and shader source
$$(SHADERS_COMPILE_$(1)): $(intermediates)/shaders/%.cgbin : $(LOCAL_PATH)/%.$(1)
	$$(transform-shader-to-cgbin)
$$(GEN_SHADERS_COMPILE_$(1)): $(intermediates)/shaders/%.cgbin : $(intermediates)/%.$(1)
	$$(transform-shader-to-cgbin)
$$(SHADERS_$(1)): $(intermediates)/shaders/%.cghex : $(intermediates)/shaders/%.cgbin
	$$(transform-cgbin-to-cghex)
$$(GEN_SHADERS_$(1)): $(intermediates)/shaders/%.cghex : $(intermediates)/shaders/%.cgbin
	$$(transform-cgbin-to-cghex)
$$(SHADERS_NOFIX_$(1)): $(intermediates)/shaders/%.h : $(intermediates)/shaders/%.cgbin
	$$(transform-cgbin-to-h)
$$(GEN_SHADERS_NOFIX_$(1)): $(intermediates)/shaders/%.h : $(intermediates)/shaders/%.cgbin
	$$(transform-cgbinr-to-h)
$$(SHADERSRC_$(1)): $(intermediates)/shaders/%.$(1)h : $(LOCAL_PATH)/%.$(1)
	$$(transform-shader-to-string)
$$(GEN_SHADERSRC_$(1)): $(intermediates)/shaders/%.$(1)h : $(intermediates)/%.$(1)
	$$(transform-shader-to-string)
endef

