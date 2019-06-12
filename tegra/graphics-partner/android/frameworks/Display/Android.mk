#
# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License. 
#

# This makefile shows how to build your own shared library that can be
# shipped on the system of a phone, and included additional examples of
# including JNI code with the library and writing client applications against it.
ifneq (,$(filter $(BOARD_INCLUDES_TEGRA_JNI),display))
LOCAL_PATH := $(call my-dir)

PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/com.nvidia.display.xml:system/etc/permissions/com.nvidia.display.xml

# the library
# ============================================================
include $(NVIDIA_DEFAULTS)

LOCAL_SRC_FILES := \
            $(call all-subdir-java-files)

# This is the target being built.
LOCAL_MODULE:= com.nvidia.display

LOCAL_ADDITIONAL_DEPENDENCIES += $(LOCAL_PATH)/com.nvidia.display.xml

include $(NVIDIA_JAVA_LIBRARY)

# the documentation
# ============================================================
include $(CLEAR_VARS)

LOCAL_SRC_FILES := $(call all-subdir-java-files) $(call all-subdir-html-files)

LOCAL_MODULE:= nvidia_display
LOCAL_DROIDDOC_OPTIONS := com.nvidia.display
LOCAL_MODULE_CLASS := JAVA_LIBRARIES
LOCAL_DROIDDOC_USE_STANDARD_DOCLET := true

include $(BUILD_DROIDDOC)

# The JNI component
# ============================================================
# Also build all of the sub-targets under this one: the library's
# associated JNI code, and a sample client of the library.

include $(LOCAL_PATH)/jni/Android.mk

endif
