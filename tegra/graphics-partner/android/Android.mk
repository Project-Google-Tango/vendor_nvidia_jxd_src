ifneq ($(HAVE_NVIDIA_PROP_SRC),false)
ifeq (,$(filter-out tegra%,$(TARGET_BOARD_PLATFORM)))
include $(all-subdir-makefiles)
endif
endif
