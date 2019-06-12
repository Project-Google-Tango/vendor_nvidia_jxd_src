# This makefile fragment generates C headers (char arrays) from binary files

# Check to ensure LOCAL_SRC_FILES is not empty
ifeq ($(LOCAL_SRC_FILES),)
$(error $(LOCAL_PATH): LOCAL_SRC_FILES cannot be empty)
endif

# Need only one elf binary as input
ifneq ($(words $(LOCAL_SRC_FILES)),1)
$(error $(LOCAL_PATH): Only one binary source file is allowed with NVIDIA_GENERATED_HEADER makefile)
endif

LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_HEADERS)

# perform some build variable intialization so that the header is treated
# as a module
include $(NVIDIA_BASE)
include $(BUILD_SYSTEM)/base_rules.mk

$(LOCAL_BUILT_MODULE): $(LOCAL_SRC_FILES)
	@echo "Generating header: $(PRIVATE_MODULE)"
	$(hide) mkdir -p $(dir $@)
	$(hide) $(NVIDIA_HEXIFY) $< $@

