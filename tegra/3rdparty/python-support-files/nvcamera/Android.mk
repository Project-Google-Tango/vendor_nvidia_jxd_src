LOCAL_PATH := $(call my-dir)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := nvcameratools.py
local_python_modules += nvcameratools.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6/nvcamera
LOCAL_SRC_FILES := nvcameratools.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := nvcamera_v1.py
local_python_modules += nvcamera_v1.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6/nvcamera_v1
LOCAL_SRC_FILES := nvcamera_v1.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := nvcamera_server.py
local_python_modules += nvcamera_server.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/share/nvcs
LOCAL_SRC_FILES := nvcamera_server.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := nvrawfile.py
local_python_modules += nvrawfile.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6
LOCAL_SRC_FILES := nvrawfile.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := nvcameraimageutils.py
local_python_modules += nvcameraimageutils.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6
LOCAL_SRC_FILES := nvcameraimageutils.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := __init__.py
local_python_modules += __init__.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6/nvcamera
LOCAL_SRC_FILES := __init__.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := version.py
local_python_modules += version.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6/nvcamera
LOCAL_SRC_FILES := version.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := __init__v1.py
local_python_modules += __init__v1.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6/nvcamera_v1
LOCAL_SRC_FILES := __init__v1.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := version_v1.py
local_python_modules += version_v1.py
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/python2.6/nvcamera_v1
LOCAL_SRC_FILES := version_v1.py
include $(NVIDIA_PREBUILT)

include $(NVIDIA_DEFAULTS)
LOCAL_MODULE := switch_nvcamera_module.sh
local_python_modules += switch_nvcamera_module.sh
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/share/nvcs
LOCAL_SRC_FILES := switch_nvcamera_module.sh
include $(NVIDIA_PREBUILT)
