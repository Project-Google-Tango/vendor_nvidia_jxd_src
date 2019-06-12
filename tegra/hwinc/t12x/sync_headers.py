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
                    SrcPath = "/home/jnazim/dev-t124/vendor/nvidia/tegra/core-private/drivers/hwinc/t12x",
                    DstPath = "/home/jnazim/dev-t124/vendor/nvidia/tegra/core/drivers/hwinc/t12x",
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


include_files.append('nvboot_aes.h')
include_files.append('nvboot_badblocks.h')
include_files.append('nvboot_bct.h')
include_files.append('nvboot_bit.h')
include_files.append('nvboot_clocks.h')
include_files.append('nvboot_config.h')
include_files.append('nvboot_devparams.h')
include_files.append('nvboot_error.h')
include_files.append('nvboot_fuse.h')
include_files.append('nvboot_hash.h')
include_files.append('nvboot_mobile_lba_nand_param.h')
include_files.append('nvboot_mux_one_nand_param.h')
include_files.append('nvboot_nand_param.h')
include_files.append('nvboot_osc.h')
include_files.append('nvboot_pmc_scratch_map.h')
include_files.append('nvboot_sdmmc_param.h')
include_files.append('nvboot_sdram_param.h')
include_files.append('nvboot_se_aes.h')
include_files.append('nvboot_se_defs.h')
include_files.append('nvboot_se_rsa.h')
include_files.append('nvboot_snor_param.h')
include_files.append('nvboot_spi_flash_param.h')
include_files.append('nvboot_usb3_param.h')

modules.append(['include', include_files])

#aes_files = []
#aes_files.append('libnvboot_aes.export')
#aes_files.append('nvboot_aes.c')
#aes_files.append('nvboot_aes_local.h')
#modules.append(['aes', aes_files])


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

