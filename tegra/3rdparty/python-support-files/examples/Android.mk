LOCAL_PATH := $(call my-dir)

local_python_module = \
    $(eval include $(NVIDIA_DEFAULTS)) \
    $(eval LOCAL_MODULE := $(strip $1)) \
    $(eval local_python_modules += $(strip $1)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_MODULE_CLASS := ETC) \
    $(eval LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/share/nvcs/examples) \
    $(eval LOCAL_SRC_FILES := $(strip $1)) \
    $(eval include $(NVIDIA_PREBUILT))

$(call local_python_module, nvcamera_capture_image.py)
