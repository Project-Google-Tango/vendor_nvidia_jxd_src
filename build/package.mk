# Let NVIDIA_BASE know that this is a package
LOCAL_MODULE_CLASS := APPS
include $(NVIDIA_BASE)
LOCAL_MODULE_CLASS :=
include $(BUILD_PACKAGE)

# BUILD_PACKAGE doesn't consider additional dependencies
$(LOCAL_BUILT_MODULE): $(LOCAL_ADDITIONAL_DEPENDENCIES)
