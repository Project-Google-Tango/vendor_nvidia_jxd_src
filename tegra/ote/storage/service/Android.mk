# Copyright (C) 2013 The Android Open Source Project
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

ifeq (tlk,$(SECURE_OS_BUILD))

LOCAL_PATH:= $(call my-dir)

## Make internal task
include $(NVIDIA_DEFAULTS)

LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
		    $(TOP)/3rdparty/ote/lib/include
LOCAL_SRC_FILES := service.c manifest.c
LOCAL_MODULE := storage_service
LOCAL_STATIC_LIBRARIES:= \
	libc \
	libtlk_common \
	libtlk_service \
	libcrypto_static

LOCAL_LDFLAGS := -T $(TOP)/3rdparty/ote/lib/task_linker_script.ld

LOCAL_NVIDIA_WARNINGS_AS_ERRORS := 1
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_UNINSTALLABLE_MODULE := true
include $(NVIDIA_EXECUTABLE)

endif # SECURE_OS_BUILD == tlk
