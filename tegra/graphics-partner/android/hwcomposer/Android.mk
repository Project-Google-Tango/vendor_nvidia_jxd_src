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

LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(NVIDIA_DEFAULTS)
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := \
	libdl \
	liblog \
	libcutils \
	libsync \
	libEGL \
	libGLESv2 \
	libhardware \
	libnvblit \
	libnvddk_2d_v2 \
	libnvddk_vic \
	libnvgr \
	libnvrm \
	libnvrm_graphics \
	libnvos \
	libui \
	libutils

LOCAL_STATIC_LIBRARIES += \
	libnvgr2dcomposer \
	libnvglcomposer \
	libnvfxmath \
	libnvviccomposer \

LOCAL_SRC_FILES := nvhwc.c
LOCAL_SRC_FILES += nvhwc_composite.c
LOCAL_SRC_FILES += nvhwc_debug.c
LOCAL_SRC_FILES += nvhwc_external.c
LOCAL_SRC_FILES += nvhwc_props.c
LOCAL_SRC_FILES += nvfb.c
LOCAL_SRC_FILES += nvfb_cursor.c
LOCAL_SRC_FILES += nvfb_didim.c
LOCAL_SRC_FILES += nvfb_hdcp.c
LOCAL_SRC_FILES += nvfl.c
LOCAL_MODULE := hwcomposer.$(TARGET_BOARD_PLATFORM)

LOCAL_C_INCLUDES += $(LOCAL_PATH)/../include
LOCAL_C_INCLUDES += $(LOCAL_PATH)/../gralloc
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/2d/include
LOCAL_C_INCLUDES += $(TEGRA_TOP)/graphics/color/nvcms/include

LOCAL_CFLAGS += -DLOG_TAG=\"hwcomposer\"
ifeq ($(BOARD_HAVE_VID_ROUTING_TO_HDMI),true)
    LOCAL_CFLAGS += -DCINEMA_MODE=1
    LOCAL_CFLAGS += -DALLOW_VIDEO_SCALING=1
endif

ifneq ($(BOARD_HDMI_MIRROR_MODE),)
    LOCAL_CFLAGS += -DHDMI_MIRROR_MODE=$(BOARD_HDMI_MIRROR_MODE)
endif

# WAR against memory underruns when HDMI is connected.
# Forces SurfaceFlinger compositing when HDMI is connected.
#  0: Use overlays.
#  1: Force SF compositing when HDMI connected.
#
# See e.g. bugs 795591, 776593.
#
ifeq ($(TARGET_TEGRA_VERSION),ap20)
    LOCAL_CFLAGS += -DHWC_FORCE_COMPOSITING_ON_HDMI=1
else ifeq ($(TARGET_TEGRA_VERSION),t20)
    LOCAL_CFLAGS += -DHWC_FORCE_COMPOSITING_ON_HDMI=1
else
    LOCAL_CFLAGS += -DHWC_FORCE_COMPOSITING_ON_HDMI=0
    LOCAL_CFLAGS += -DALLOW_VIDEO_SCALING=1
endif

ifeq ($(BOARD_HAS_NVDPS), true)
    LOCAL_CFLAGS += -DNVDPS_ENABLE=1
else
    LOCAL_CFLAGS += -DNVDPS_ENABLE=0
endif

# This config can be used to reduce memory usage at the cost of performance.
ifeq ($(BOARD_DISABLE_TRIPLE_BUFFERED_DISPLAY_SURFACES),true)
    LOCAL_CFLAGS += -DNVGR_USE_TRIPLE_BUFFERING=0
else
    LOCAL_CFLAGS += -DNVGR_USE_TRIPLE_BUFFERING=1
endif

ifeq ($(BOARD_VENDOR_HDCP_ENABLED),true)
    LOCAL_CFLAGS += -DNV_HDCP=1
    LOCAL_C_INCLUDES += $(TOP)/$(BOARD_VENDOR_HDCP_PATH)/include
    # Enable SECURE HDCP
    # libtlk_secure_hdcp_up and libsecure_hdcp_up both provide functionality to
    # check HDCP status. One will use TLK secureos and the other use
    # TL secureos to securely check HDCP status.
    #
    # Default libhdcp_up will be used if secureos is not available
    ifeq ($(BOARD_ENABLE_SECURE_HDCP),1)
        ifeq ($(SECURE_OS_BUILD),tlk)
            LOCAL_SHARED_LIBRARIES += libtlk_secure_hdcp_up
        else
            LOCAL_SHARED_LIBRARIES += libsecure_hdcp_up
        endif
    else
        LOCAL_STATIC_LIBRARIES += libhdcp_up
        LOCAL_SHARED_LIBRARIES += libpkip libcrypto
    endif
    ifneq ($(BOARD_VENDOR_HDCP_INTERVAL),)
        LOCAL_CFLAGS += -DNV_HDCP_POLL_INTERVAL=$(BOARD_VENDOR_HDCP_INTERVAL)
    endif
endif

ifneq ($(BOARD_HDMI_RESOLUTION_MODE),)
    LOCAL_CFLAGS += -DNV_PROPERTY_HDMI_RESOLUTION_DEF=$(BOARD_HDMI_RESOLUTION_MODE)
endif

ifeq ($(BOARD_ENABLE_NV_ATRACE),true)
    LOCAL_CFLAGS += -DNV_ATRACE=1
endif

LOCAL_CFLAGS += -DNVCAP_VIDEO_ENABLED=1

include $(NVIDIA_SHARED_LIBRARY)
