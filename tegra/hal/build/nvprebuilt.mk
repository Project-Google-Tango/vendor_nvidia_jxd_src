
##
## This file contains special rules for handling prebuilt modules
## supplied by NVIDIA.
##
## The Android prebuilt target copies the prebuilt files directly
## into the output directory, without placing appropriate copies
## for debugging and linking against. Also prebuilt files don't
## get stripped for the target.
##
## TODO: support for actual target executables, once we need them.

ifeq ($(NV_CLASS),SHARED_LIBRARIES)

## NVIDIA prebuilt shared libraries

include $(CLEAR_VARS)

LOCAL_MODULE 	           := $(basename $(notdir $(NV_FILE)))
LOCAL_PATH                 := $(NV_TOP_PATH)/$(NV_PATH)
LOCAL_MODULE_SUFFIX  	   := $(TARGET_SHLIB_SUFFIX)
LOCAL_MODULE_CLASS         := SHARED_LIBRARIES
OVERRIDE_BUILT_MODULE_PATH := $(TARGET_OUT_INTERMEDIATE_LIBRARIES)

RELATIVE_DIR := $(dir $(NV_FILE))
RELATIVE_DIR := $(RELATIVE_DIR:/=)

ifneq ($(RELATIVE_DIR),.)
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/$(RELATIVE_DIR)
endif

include $(BUILD_SYSTEM)/dynamic_binary.mk

$(linked_module): $(NV_TOP_PATH)/$(NV_PATH)/$(NV_FILE)
	$(copy-file-to-target)	

endif

ifeq ($(NV_CLASS),STATIC_LIBRARIES)

## NVIDIA prebuilt static libraries

include $(CLEAR_VARS)

LOCAL_MODULE 	           := $(basename $(notdir $(NV_FILE)))
LOCAL_PATH                 := $(NV_TOP_PATH)/$(NV_PATH)
LOCAL_MODULE_SUFFIX  	   := .a
LOCAL_MODULE_CLASS         := STATIC_LIBRARIES

include $(BUILD_SYSTEM)/binary.mk

$(LOCAL_BUILT_MODULE): $(NV_TOP_PATH)/$(NV_PATH)/$(NV_FILE)
	$(copy-file-to-target)	

endif

ifneq ($(filter $(NV_CLASS),EXECUTABLES),)

## Everything else

include $(CLEAR_VARS)

LOCAL_PATH := $(NV_TOP_PATH)/$(NV_PATH)
$(call add-prebuilt-file, $(NV_FILE), $(NV_CLASS)) 

endif
