# See build/core/raw_executable.mk for the original.
# Supported variables from module
# LOCAL_NVIDIA_LINK_SCRIPT
# LOCAL_NVIDIA_LINK_SCRIPT_PATH
# LOCAL_NVIDIA_RAW_EXECUTABLE_LDFLAGS
# LOCAL_NVIDIA_OBJCOPY_FLAGS
# LOCAL_NVIDIA_USE_PUBLIC_KEY_HEADER

LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_MODULE_SUFFIX := .bin
LOCAL_SYSTEM_SHARED_LIBRARIES :=

include $(NVIDIA_BASE)
include $(NVIDIA_COVERAGE)
include $(BUILD_SYSTEM)/binary.mk

# Public key header target
# Generate the publickey.h file based on signkey.pk8
_public_key_header := $(TARGET_OUT_HEADERS)/publickey.h
_signkey := $(TOP)/device/nvidia/common/security/signkey.pk8
_nvdumpublickey := $(HOST_OUT_EXECUTABLES)/nvdumppublickey

$(_public_key_header): $(_nvdumpublickey) $(_signkey)
	$(hide)$(filter %nvdumppublickey,$^) $(filter %signkey.pk8,$^) $@

# Provision to use module defined linker script
ifeq ($(LOCAL_NVIDIA_LINK_SCRIPT),)
LOCAL_NVIDIA_LINK_SCRIPT := $(LOCAL_NVIDIA_LINK_SCRIPT_PATH)/$(LOCAL_MODULE).x
endif

$(LOCAL_BUILT_MODULE) : PRIVATE_TARGET_CC := $(TARGET_CC)
$(LOCAL_BUILT_MODULE) : PRIVATE_TARGET_LD := $(TARGET_LD)
$(LOCAL_BUILT_MODULE) : PRIVATE_ELF_FILE := $(intermediates)/$(PRIVATE_MODULE).elf
$(LOCAL_BUILT_MODULE) : PRIVATE_LINK_SCRIPT := $(LOCAL_NVIDIA_LINK_SCRIPT)
$(LOCAL_BUILT_MODULE) : PRIVATE_RAW_EXE_LDFLAGS := -static --entry=_start --gc-sections
$(LOCAL_BUILT_MODULE) : PRIVATE_RAW_EXE_LDFLAGS += $(LOCAL_NVIDIA_RAW_EXECUTABLE_LDFLAGS)
$(LOCAL_BUILT_MODULE) : PRIVATE_OBJCOPY_FLAGS := $(LOCAL_NVIDIA_OBJCOPY_FLAGS)

# Provision _not_ to use public key header, by default any module using this
# makefile fragment will use public header key
ifneq ($(LOCAL_NVIDIA_USE_PUBLIC_KEY_HEADER),false)
$(all_objects): $(_public_key_header)
endif

$(all_objects) : TARGET_PROJECT_INCLUDES :=
$(all_objects) : TARGET_C_INCLUDES :=
$(all_objects) : TARGET_GLOBAL_CFLAGS :=
$(all_objects) : TARGET_GLOBAL_CPPFLAGS :=

$(LOCAL_BUILT_MODULE): $(all_objects) $(all_libraries) $(LOCAL_NVIDIA_LINK_SCRIPT)
	@echo "target Linking static executable: $(PRIVATE_MODULE)"
	$(hide)mkdir -p $(dir $@)
	$(hide)$(PRIVATE_TARGET_LD) \
		$(addprefix --script ,$(PRIVATE_LINK_SCRIPT)) \
		$(PRIVATE_RAW_EXE_LDFLAGS) \
		-o $(PRIVATE_ELF_FILE) \
		$(PRIVATE_ALL_OBJECTS) \
		--start-group $(PRIVATE_ALL_STATIC_LIBRARIES) --end-group
	$(hide)$(TARGET_OBJCOPY) -O binary $(PRIVATE_OBJCOPY_FLAGS) $(PRIVATE_ELF_FILE) $@

# Clear local variables
_public_key_header :=
_signkey :=
_nvdumpublickey :=

