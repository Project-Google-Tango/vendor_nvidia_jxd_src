#
# Nvidia specific targets
#

.PHONY: dev nv-blob sim-image list-non-nv-modules

dev: droidcore target-files-package
ifneq ($(NO_ROOT_DEVICE),)
	device/nvidia/common/generate_nvtest_ramdisk.sh $(TARGET_PRODUCT) $(TARGET_BUILD_TYPE)
	device/nvidia/common/generate_qt_ramdisk.sh $(TARGET_PRODUCT) $(TARGET_BUILD_TYPE)
	device/nvidia/common/generate_full_filesystem.sh
	device/nvidia/common/generate_asim_bootimg.sh
endif

#
# bootloader blob target and macros
#

# macro: checks file existence and returns list of existing file
# $(1) list of file paths
define _dynamic_blob_dependencies
$(foreach f,$(1), $(eval \
 ifneq ($(wildcard $(f)),)
  _dep += $(f)
 endif))\
 $(_dep)
 $(eval _dep :=)
endef

# macro: construct command line for nvblob based on type of input file
# $(1) list of file paths
define _blob_command_line
$(foreach f,$(1), $(eval \
 ifneq ($(filter %microboot.bin,$(f)),)
  _cmd += $(f) NVC 1
  _cmd += $(f) RMB 1
 else ifneq ($(filter %.dtb,$(f)),)
  _cmd += $(f) DTB 1
 else ifneq ($(filter %.bct,$(f)),)
  _cmd += $(f) BCT 1
 else ifneq ($(filter %xusb_sil_rel_fw,$(f)),)
  _cmd += $(f) DFI 1
 endif))\
 $(_cmd)
 $(eval _cmd :=)
endef

# These are additional files for which we generate blobs only if they exists
_blob_deps := \
      $(PRODUCT_OUT)/microboot.bin \
      $(wildcard $(PRODUCT_OUT)/$(TARGET_KERNEL_DT_NAME)*.dtb) \
      $(PRODUCT_OUT)/flash.bct \
      $(PRODUCT_OUT)/nvtboot.bin \
      $(PRODUCT_OUT)/xusb_sil_rel_fw

# target to generate blob
nv-blob: \
      $(HOST_OUT_EXECUTABLES)/nvblob \
      $(HOST_OUT_EXECUTABLES)/nvsignblob \
      $(TOP)/device/nvidia/common/security/signkey.pk8 \
      $(PRODUCT_OUT)/bootloader.bin \
      $(call _dynamic_blob_dependencies, $(_blob_deps))
	$(hide) python $(filter %nvblob,$^) \
		$(filter %bootloader.bin,$^) EBT 1 \
		$(filter %bootloader.bin,$^) RBL 1 \
		 $(call _blob_command_line, $^)

#
# Generate ramdisk images for simulation
#
sim-image: nvidia-tests
	device/nvidia/common/copy_simtools.sh
	device/nvidia/common/generate_full_filesystem.sh
	@echo "Generating sdmmc image w/ full filesystem ..."
	device/nvidia/common/sdmmc_util.sh \
	    -s 2048 -z \
	    -o $(PRODUCT_OUT)/sdmmc_full_fs.img \
	    -c device/nvidia/common/sdmmc_full_fs.cfg

# This macro lists all modules filtering those which
# 1. Are in a path which contains 'nvidia'
# 2. Have dependencies which are in a path which contains 'nvidia'
# TODO: This doesn't work well if a dependency have same name but different
# class. Eg. libexpat which is defined in multiple makefiles as host shared
# lib, shared lib and static lib
define list_nv_independent_modules
$(foreach _m,$(call module-names-for-tag-list,$(ALL_MODULE_TAGS)), \
    $(if $(findstring nvidia,$(ALL_MODULES.$(_m).PATH)), \
        $(info Skipping $(_m) location : $(ALL_MODULES.$(_m).PATH)), \
        $(if $(strip $(ALL_MODULES.$(_m).REQUIRED)), \
	    $(foreach _d,$(ALL_MODULES.$(_m).REQUIRED), \
	        $(if $(findstring nvidia,$(ALL_MODULES.$(_d).PATH)), \
	            $(info Skipping $(_m) location : $(ALL_MODULES.$(_m).PATH) dependency : $(_d) dependency location : $(ALL_MODULES.$(_d).PATH)), \
	            $(_m) \
	        ) \
	    ), \
	    $(_m) \
        ) \
    ) \
)
endef

# List all nvidia independent modules as well as modules skipped with reason
list-non-nv-modules:
	@echo "Nvidia independent modules analysis:"
	@for m in $(call list_nv_independent_modules); do echo $$m; done | sort -u

# Clear local variable
_blob_deps :=
