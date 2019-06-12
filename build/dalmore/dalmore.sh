#
# Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

# NVIDIA Tegra4 "Dalmore" development system
OUTDIR=$(get_abs_build_var PRODUCT_OUT)
echo "DEBUG: PRODUCT_OUT = $OUTDIR"

# setup FASTBOOT VENDOR ID
export FASTBOOT_VID=0x955
# Set ODM_DATA for 1GB SDRAM
if [ "$ODMDATA_OVERRIDE" ]; then
    export NVFLASH_ODM_DATA=$ODMDATA_OVERRIDE
else
    export NVFLASH_ODM_DATA=0x00098000
fi

if [ "$BOARD_IS_E1613" == "1" ]
then
      cp $OUTDIR/flash_dalmore_e1613.bct $OUTDIR/flash.bct
      cp $OUTDIR/flash_dalmore_e1613.cfg $OUTDIR/bct.cfg
else
      cp $OUTDIR/flash_dalmore_e1611.bct $OUTDIR/flash.bct
      cp $OUTDIR/flash_dalmore_e1611.cfg $OUTDIR/bct.cfg
fi
