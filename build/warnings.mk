###############################################################################
#
# Copyright (c) 2011-2012 NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.
#
###############################################################################
# warnings.mk:
# Enables extra warnings and all warnings as errors for nvidia modules. These
# are enabled for L4T builds and should always be enabled for Android as well.
# There are four variables to control the behaviour from module's Android.mk:
# LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS :=1 turns off errors for all warnings
# LOCAL_NVIDIA_NO_EXTRA_WARNINGS := 1 turns off extra warnings introduced here
# LOCAL_NVIDIA_NO_EXTRA_WARNINGS_AS_ERRORS := 1 turns off error for warnings
# introduced here
# LOCAL_NVIDIA_RM_WARNING_FLAGS := <flag>
# eg: LOCAL_NVIDIA_RM_WARNING_FLAGS := -Wundef --  removes the warning flag.
# In some cases like in cases, -Wundef, -Wno-error=undef doesn't work
# LOCAL_NVIDIA_RM_WARNING_FLAGS can be used to filter such cases
#
###############################################################################
# IMPORTANT: A good module should never have any of these four variables set.
# All the four variables must be removed from all Android.mk files, and then
# their processing removed from here so that inconsistency in warning flags in
# L4T and Android builds never break our builds.
###############################################################################

# Returns list of sources having .cpp extenstion
_module_has_cpp_sources = $(strip $(filter %.cpp,$(LOCAL_SRC_FILES)))

# nested-externs & redundant-decls produce following errors -- commented out
# unistd.h: In function 'getpagesize':
# unistd.h:171: error: nested extern declaration of '__page_size'
# unistd.h: In function '__getpageshift':
# unistd.h:175: error: nested extern declaration of '__page_shift'
# strings.h:50: error: redundant redeclaration of 'index'
# strings.h:51: error: redundant redeclaration of 'rindex'
# strings.h:52: error: redundant redeclaration of 'strcasecmp'
# strings.h:53: error: redundant redeclaration of 'strncasecmp'

ifneq ($(LOCAL_NVIDIA_NO_EXTRA_WARNINGS),1)
LOCAL_CFLAGS += -Wmissing-declarations
LOCAL_CFLAGS += -Wundef
#LOCAL_CFLAGS += -Wredundant-decls

# There are many modules with unused warnings (about 1500 warnings in total)
# commented out
#LOCAL_CFLAGS += -Wunused

# Warnings only valid for C, not for CPP
ifeq (,$(call _module_has_cpp_sources))
#LOCAL_CFLAGS += -Wnested-externs
LOCAL_CFLAGS += -Wmissing-prototypes
LOCAL_CFLAGS += -Wstrict-prototypes
endif

# Currently there is a bug in sys/cdefs.h, this will satisfy the compiler
# http://code.google.com/p/android/issues/detail?id=14627
LOCAL_CFLAGS += -D__STDC_VERSION__=0
endif

# Turn on warnings as errors
ifneq ($(LOCAL_NVIDIA_NO_WARNINGS_AS_ERRORS),1)
LOCAL_CFLAGS += -Wno-error=maybe-uninitialized
LOCAL_CFLAGS += -Wno-error=narrowing
LOCAL_CFLAGS += -Wno-error=missing-field-initializers
# Add -Werror only if it is not already present
ifeq (,$(findstring -Werror, $(LOCAL_CFLAGS)))
LOCAL_CFLAGS += -Werror
endif
endif

LOCAL_LDFLAGS += -Wl,--no-fatal-warnings

# Filter out flags defined in LOCAL_NVIDIA_RM_WARNING_FLAGS from LOCAL_CFLAGS.
# Only used for -Wundef as it can not be turned off with -Wno-error=undef
ifneq (,$(LOCAL_NVIDIA_RM_WARNING_FLAGS))
LOCAL_CFLAGS := $(filter-out $(LOCAL_NVIDIA_RM_WARNING_FLAGS),$(LOCAL_CFLAGS))
endif

# Variable cleanup
_module_has_cpp_sources :=
