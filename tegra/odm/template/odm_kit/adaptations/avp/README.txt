#
# Copyright (c) 2010 NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

Bootloader initial boot sequence
--------------------------------
Tegra Bootloader is booted as follows.

    1. After reset, Boot ROM runs on AVP.
    2. Boot ROM loads Bootloader on DRAM.
    3. Boot ROM jumps to the Bootloader entry.
    4. Bootloader starts on AVP.
    5. AVP initializes and starts CPU.
    6. Bootloader runs on CPU.
    7. AVP halts.

There are some cases small amount of preparation is necessary before the
step 5; such as, turning on CPU power rail.

AVP ODM functions provides the programming environment for such purpose.

AVP ODM functions
-----------------
The file nvodm_avp.c defines the following two functions.

    NvOdmAvpPreCpuInit()
    NvOdmAvpPostCpuInit()

NvOdmAvpPreCpuInit() is called by AVP right before the step 5, and
NvOdmAvpPostCpuInit() is called right after the step 5.

Customers can write those functions. Those functions are empty by default.

Those functions are compiled for AVP (armv4), and linked with the
Bootloader which is compiled for CPU (armv6).

ODM services
------------
The following functions can be called from the AVP ODM functions.

    NvOdmI2cOpen()
    NvOdmI2cClose()
    NvOdmI2cTransaction()

Their definitions are in nvodm_services.h.

Those functions are not fully implemented; only enough to access PMU
device on the PMU I2C bus. Those functions can be used for other I2C
devices on the same I2C bus.

For LDK release users
---------------------
On the LDK release, the source file of the AVP ODM functions is in the
following file.

    bdk/odm_kit/whistler/adaptations/avp/nvodm_avp.c

After modifying the file, you can recompile the bootloader as usual.

    > cd bdk
    > make BUILD_TOOLS_DIR=/<path to>/codesourcery/arm-2009q1-161-arm-none-eabi

The binaries (bdk/fastboot and bdk/fastboot.bin) are built with your
AVP ODM functions.

Limitation
----------
RVDS is not supported. GCC has to be used to compile Bootloader.
