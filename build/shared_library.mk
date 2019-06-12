LOCAL_MODULE_CLASS := SHARED_LIBRARIES

include $(NVIDIA_BASE)
include $(NVIDIA_WARNINGS)
include $(NVIDIA_COVERAGE)

LOCAL_LDFLAGS += -Wl,--build-id=sha1

# try guessing the .export file if not given
ifeq ($(LOCAL_NVIDIA_EXPORTS),)
LOCAL_NVIDIA_EXPORTS := $(strip $(wildcard $(LOCAL_PATH)/$(LOCAL_MODULE)_*.export) $(wildcard $(LOCAL_PATH)/$(LOCAL_MODULE).export))
else
LOCAL_NVIDIA_EXPORTS := $(addprefix $(LOCAL_PATH)/,$(LOCAL_NVIDIA_EXPORTS))
endif

# if .export files are given, add linker script to linker options
ifneq ($(LOCAL_NVIDIA_EXPORTS),)
LOCAL_LDFLAGS += -Wl,--version-script=$(intermediates)/$(LOCAL_MODULE).script
endif

include $(BUILD_SHARED_LIBRARY)

# rule for building the linker script
ifneq ($(LOCAL_NVIDIA_EXPORTS),)
$(intermediates)/$(LOCAL_MODULE).script: $(LOCAL_NVIDIA_EXPORTS) $(NVIDIA_GETEXPORTS)
	@mkdir -p $(dir $@)
	@echo "Generating linker script $@"
	$(hide) python $(NVIDIA_GETEXPORTS) -script none none none $(filter %.export,$^) > $@ 
$(linked_module): $(intermediates)/$(LOCAL_MODULE).script

# rule for building the apicheck executable
ifeq ($(NVIDIA_APICHECK),1)
NVIDIA_CHECK_MODULE := $(LOCAL_MODULE)
NVIDIA_CHECK_MODULE_LINK := $(LOCAL_BUILT_MODULE)
include $(CLEAR_VARS)
LOCAL_MODULE := $(NVIDIA_CHECK_MODULE)_apicheck
LOCAL_MODULE_CLASS := EXECUTABLES
intermediates := $(local-intermediates-dir)
LOCAL_MODULE_PATH := $(intermediates)/CHECK
LOCAL_MODULE_TAGS := optional
LOCAL_GENERATED_SOURCES += $(intermediates)/check.c
LOCAL_LDFLAGS += -l$(patsubst lib%,%,$(NVIDIA_CHECK_MODULE))
include $(BUILD_EXECUTABLE)
$(linked_module): $(NVIDIA_CHECK_MODULE_LINK)
$(intermediates)/check.c: $(LOCAL_NVIDIA_EXPORTS) $(NVIDIA_GETEXPORTS)
	@mkdir -p $(dir $@)
	@echo "Generating apicheck source $@"
	$(hide) python $(NVIDIA_GETEXPORTS) -apicheck none none none $(filter %.export,$^) > $@ 
# restore some of the variables for potential further use in caller
LOCAL_MODULE := $(NVIDIA_CHECK_MODULE)
LOCAL_BUILT_MODULE := $(NVIDIA_CHECK_MODULE_LINK)
endif
endif
