LOCAL_PATH := $(call my-dir)

local_python_src_libs := $(patsubst $(LOCAL_PATH)/%,%, \
                                   $(wildcard $(LOCAL_PATH)/*.py) \
                                   $(wildcard $(LOCAL_PATH)/*/*.py))
$(foreach file, $(local_python_src_libs), \
    $(eval include $(NVIDIA_DEFAULTS)) \
    $(eval LOCAL_MODULE := $(strip $(file))) \
    $(eval local_python_modules +=  $(strip $(file))) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_MODULE_CLASS := SHARED_LIBRARIES) \
    $(eval LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6) \
    $(eval LOCAL_SRC_FILES := $(strip $(file))) \
    $(eval include $(NVIDIA_PREBUILT)))
