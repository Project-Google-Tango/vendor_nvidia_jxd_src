LOCAL_PATH := $(call my-dir)

local_test_module = \
    $(eval include $(NVIDIA_DEFAULTS)) \
    $(eval LOCAL_MODULE := $(strip $1)) \
    $(eval local_python_modules += $(strip $1)) \
    $(eval LOCAL_MODULE_TAGS := optional) \
    $(eval LOCAL_MODULE_CLASS := ETC) \
    $(eval LOCAL_MODULE_PATH := $(TARGET_OUT)/usr/share/nvcs/tests) \
    $(eval LOCAL_SRC_FILES := $(strip $1)) \
    $(eval include $(NVIDIA_PREBUILT))

$(call local_test_module, nvcstestmain.py)
$(call local_test_module, nvcsflashtests.py)
$(call local_test_module, nvcsfocusertests.py)
$(call local_test_module, nvcshostsensortests.py)
$(call local_test_module, nvcssensortests.py)
$(call local_test_module, nvcstestcore.py)
$(call local_test_module, nvcstestutils.py)
$(call local_test_module, nvcstest.py)
$(call local_test_module, README.txt)
