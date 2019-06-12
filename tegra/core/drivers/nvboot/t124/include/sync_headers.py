#!/usr/bin/env python

#
# sync_tree.py
#
# As its name implies, sync_tree.py syncs files.  In this case, it copies
# Boot ROM source files from the HW tree to the SW tree.
#
# The arguments handled by sync_tree:
# -s, --src_path: Sets the path to the source directory.  This should be
#                 //hw/ap/drv/bootrom/nvboot
# -d, --dst_path: Sets the path to the destination directory.  This should be
#                 //sw/mobile/main/drivers/nvboot/t12x
#

import os, sys
import subprocess
from optparse import OptionParser

# Build up the command line parser

parser = OptionParser()

parser.set_defaults(
                    SrcPath = "/home/jnazim/dev-t124/vendor/nvidia/tegra/microboot/nvboot/t124/include/t124",
                    DstPath = "/home/jnazim/dev-t124/vendor/nvidia/tegra/core/drivers/nvboot/t124/include",
                    )

parser.add_option("-d", "--dst_path",
                  action="store",
                  dest="DstPath",
                  help="Selects the destination path. Should point to //sw/mobile/main/drivers/nvboot/t12x.")

parser.add_option("-s", "--src_path",
                  action="store",
                  dest="SrcPath",
                  help="Selects the source path. Should point to //hw/ap/drv/bootrom/nvboot.")

(options, args) = parser.parse_args()

#
# Prepare list of (module, filelist) pairs of files to process.
#
modules = []

include_files = []
include_files.append('nvboot_aes_int.h')
include_files.append('nvboot_arc_int.h')
include_files.append('nvboot_ahb_int.h')
include_files.append('nvboot_badblocks_int.h')
include_files.append('nvboot_bct_int.h')
include_files.append('nvboot_bootloader_int.h')
include_files.append('nvboot_buffers_int.h')
include_files.append('nvboot_clocks_int.h')
include_files.append('nvboot_codecov_int.h')
include_files.append('nvboot_coldboot_int.h')
include_files.append('nvboot_config_int.h')
include_files.append('nvboot_context_int.h')
include_files.append('nvboot_device_int.h')
include_files.append('nvboot_devmgr_int.h')
include_files.append('nvboot_fuse_int.h')
include_files.append('nvboot_hacks_int.h')
include_files.append('nvboot_hardware_access_int.h')
include_files.append('nvboot_hash_int.h')
include_files.append('nvboot_irom_patch_int.h')
include_files.append('nvboot_limits_int.h')
include_files.append('nvboot_main_int.h')
include_files.append('nvboot_pads_int.h')
include_files.append('nvboot_pmc_int.h')
include_files.append('nvboot_rcm_int.h')
include_files.append('nvboot_reader_int.h')
include_files.append('nvboot_reset_int.h')
include_files.append('nvboot_reset_pmu_int.h')
include_files.append('nvboot_sdmmc_context.h')
include_files.append('nvboot_sdmmc_int.h')
include_files.append('nvboot_sdram_int.h')
include_files.append('nvboot_se_int.h')
include_files.append('nvboot_sku_int.h')
include_files.append('nvboot_snor_context.h')
include_files.append('nvboot_snor_int.h')
include_files.append('nvboot_spi_flash_int.h')
include_files.append('nvboot_spi_flash_context.h')
include_files.append('nvboot_ssk_int.h')
include_files.append('nvboot_strap_int.h')
include_files.append('nvboot_uart_int.h')
include_files.append('nvboot_usbf_int.h')
include_files.append('nvboot_util_int.h')
include_files.append('nvboot_warm_boot_0_int.h')
include_files.append('nvboot_wdt_int.h')

modules.append(['include', include_files])

#aes_files = []
#aes_files.append('libnvboot_aes.export')
#aes_files.append('nvboot_aes.c')
#aes_files.append('nvboot_aes_local.h')
#modules.append(['aes', aes_files])

#ahb_files = []
#ahb_files.append('libnvboot_ahb.export')
#ahb_files.append('nvboot_ahb.c')
#modules.append(['ahb', ahb_files])
#
#arc_files = []
#arc_files.append('libnvboot_arc.export')
#arc_files.append('nvboot_arc.c')
#modules.append(['arc', arc_files])
#
#clocks_files = []
#clocks_files.append('libnvboot_clocks.export')
#clocks_files.append('nvboot_clocks.c')
#modules.append(['clocks', clocks_files])

#fuse_files = []
#fuse_files.append('libnvboot_fuse.export')
#fuse_files.append('nvboot_fuse.c')
#fuse_files.append('nvboot_fuse_local.h')
#modules.append(['fuse', fuse_files])
#
#hash_files = []
#hash_files.append('libnvboot_hash.export')
#hash_files.append('nvboot_hash.c')
#modules.append(['hash', hash_files])
#
#pads_files = []
#pads_files.append('libnvboot_pads.export')
#pads_files.append('nvboot_pads.c')
#pads_files.append('nvboot_pinmux.c')
#pads_files.append('nvboot_pinmux_local.h')
#pads_files.append('nvboot_pinmux_tables.c')
#modules.append(['pads', pads_files])
#
#pmc_files = []
#pmc_files.append('libnvboot_pmc.export')
#pmc_files.append('nvboot_pmc.c')
#modules.append(['pmc', pmc_files])
#
#rcm_files = []
#rcm_files.append('libnvboot_rcm.export')
#rcm_files.append('nvboot_rcm.c')
#modules.append(['rcm', rcm_files])
#
#reset_files = []
#reset_files.append('libnvboot_reset.export')
#reset_files.append('nvboot_reset.c')
#reset_files.append('nvboot_reset_pmu.c')
#reset_files.append('nvboot_reset_pmu_stub.c')
#modules.append(['reset', reset_files])
#
#se_files = []
#se_files.append('libnvboot_se.export')
#se_files.append('nvboot_se.c')
#modules.append(['se', se_files])
#
#sdmmc_files = []
#sdmmc_files.append('libnvboot_sdmmc.export')
#sdmmc_files.append('nvboot_sdmmc.c')
#sdmmc_files.append('nvboot_sdmmc_local.h')
#modules.append(['sdmmc', sdmmc_files])
#
#sdram_files = []
#sdram_files.append('libnvboot_sdram.export')
#sdram_files.append('nvboot_sdram.c')
#sdram_files.append('nvboot_sdram_wrapper.c')
#modules.append(['sdram', sdram_files])
#
#sku_files = []
#sku_files.append('libnvboot_sku.export')
#sku_files.append('nvboot_sku.c')
#modules.append(['sku', sku_files])
#
#snor_files = []
#snor_files.append('libnvboot_snor.export')
#snor_files.append('nvboot_snor.c')
#snor_files.append('nvboot_snor_local.h')
#modules.append(['snor', snor_files])
#
#spi_flash_files = []
#spi_flash_files.append('libnvboot_spi_flash.export')
#spi_flash_files.append('nvboot_spi_flash.c')
#modules.append(['spi_flash', spi_flash_files])
#
#ssk_files = []
#ssk_files.append('libnvboot_ssk.export')
#ssk_files.append('nvboot_ssk.c')
#modules.append(['ssk', ssk_files])
#
#usbf_files = []
#usbf_files.append('libnvboot_usbf.export')
#usbf_files.append('nvboot_usbf.c')
#usbf_files.append('nvboot_usbf_common.c')
#usbf_files.append('nvboot_usbf_hw.h')
#modules.append(['usbf', usbf_files])
#
#util_files = []
#util_files.append('libnvboot_util.export')
#util_files.append('nvboot_util.c')
#modules.append(['util', util_files])
#
#warm_boot_0_files = []
#warm_boot_0_files.append('libnvboot_warm_boot_0.export')
#warm_boot_0_files.append('nvboot_warm_boot_0.c')
#warm_boot_0_files.append('nvboot_wb0_sdram_pack.c')
#warm_boot_0_files.append('nvboot_wb0_sdram_unpack.c')
#modules.append(['warm_boot_0', warm_boot_0_files])
#
#wdt_files = []
#wdt_files.append('libnvboot_wdt.export')
#wdt_files.append('nvboot_wdt.c')
#modules.append(['wdt', wdt_files])

#
# Copy and filter the files
#
for module, files in modules:
    for file in files:
        src = options.SrcPath + '/' + file
        dst = options.DstPath + '/' + file
	p = subprocess.Popen(['diff', '--ignore-space-change',src,dst],shell=False,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
	diff = p.stdout.read()
	if len(diff):
		print diff
		print src
        # Open and read the input file.
        f_in = open(src, 'r')
        data = f_in.readlines()
        f_in.close()

        # Open the output file
        f_out = open(dst, 'w')

        # At present, simply copy the data.  Any desired processing can
            # be applied herein.
        for line in data:
           f_out.write(line)

        # Close the output file
        f_out.close()

