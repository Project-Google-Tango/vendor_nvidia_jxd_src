# Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

TargetType="e1859_t114"
BoardKernArgs="usbcore.old_scheme_first=1"
BoardOptions="k:p:v:"
PartitionLayout=
UserConfigFile=
ConfigBootMedium=0
QNXFileNamePrefix="ifs-nvidia-t11x"
QNXFileNameBoardName="e1859_t114"
QNXFileNameVariant="profiler"
ValidSkus=( 1859 )
SkuValid=0

BoardParseOptions()
{

    case $Option in
        k) ConfigFile=$OPTARG
           if [ ! -e $ConfigFile ]; then
               echo "Config file $ConfigFile does not exist"
               exit -1
           fi
           UserConfigFile=1
           ;;
        p) PartitionLayout=$OPTARG
           ;;
        v) QNXFileNameVariant=$OPTARG
           ;;
        *) echo "Invalid option $Option"
           Usage
           AbnormalTermination
           ;;
    esac
}

#functions needed by burnflash.sh

BoardHelpHeader()
{
    echo "Usage : `basename $0` [ -h ]  <-n <NFSPORT> | -r <ROOTDEVICE> > [ options ]"
}

BoardHelpOptions()
{
    echo "-S : Provide SKU id from amongst the following: `echo ${ValidSkus[@]} | awk -v var=', ' '{ gsub(/ /,var,$0); print }'` "
    echo "-k : Provide the nvflash config file to be used."
    echo "     Default value is quickboot_nand_linux.cfg for nor boot."
    echo "-p : Provide the partition Layout for the device in the mtdpart format."
    echo "     Partition start and size should be block size aligned"
    echo "     example: mtdparts:<DRIVER_NAME>:size@start(PART_NAME)"
    echo "            Fastboot  \"mtdparts=tegra-nor:512K@17920K(env),14336K@18432K(userspace) \""
    echo "            Quickboot \"mtdparts=tegra-nor:256K@21248K(env),11264K@21504K(userspace) \""
    echo "-v : QNX image variant: profiler (default and only option supported)"
    echo ""
#FIXME: Change this for DevAir
    echo "Supported <NFSPORT>    : usb0 : Ethernet connector J58 on E1888 (baseboard)"
    echo "                       : eth0 : USB-Ethernet adapter"
    echo "                       Supported USB-Ethernet adapters: D-Link DUB-E100, Linksys USB200M ver.2.1"
}

BoardValidateParams()
{
    # if SKU file is specified, extract sku id from the filename
    if [ -n "${ZFile}" ]; then
        SkuId=`echo $ZFile|cut -d '-' -f 3`
    fi

    if [ $SkuId -eq 0 ]; then
        echo "Missing -S option (mandatory)"
        Usage
        AbnormalTermination
    fi

    if [[ "$QNXFileNameVariant" != "profiler" ]]; then
        echo "Invalid QNX image variant" $QNXFileNameVariant
        AbnormalTermination
    fi

    BCT=${NvFlashDir}/E1859_Hynix_4GB_H5TC4G63AFR-PBA_756MHz_NAND.bct

    if [ ${RcmMode} -eq 1 ];then
       BCT=${NvFlashDir}/E1859_Hynix_4GB_H5TC4G63AFR-PBA_756MHz_NAND.bct
    fi

    let "ODMData = 0x80080000" # UART-A (Lower J7)

    if [ -z $ConfigFile ]; then
        if [ ! ${FlashQNX} -eq 1 ]; then
            if [ $Quickboot -eq 0 ] ; then
                ConfigFile=${NvFlashDir}/fastboot_snor_linux.cfg
            else
                ConfigFile=${NvFlashDir}/quickboot_snor_linux.cfg
            fi
            if [ -z ${PartitionLayout} ]; then
                if [ $Quickboot -eq 0 ] ; then
                    PartitionLayout="mtdparts=tegra-nor:512K@17920K(env),14336K@18432K(userspace) "
                else
                    PartitionLayout="mtdparts=tegra-nor:256K@21248K(env),11264K@21504K(userspace) "
                fi
            fi
        else
            if [ $Quickboot -eq 0 ] ; then
                ConfigFile=${NvFlashDir}/quickboot_nand_qnx.cfg
            else
                ConfigFile=${NvFlashDir}/quickboot_nand_qnx.cfg
            fi
        fi
    fi
    BoardKernArgs="${BoardKernArgs} ${PartitionLayout}"
    QNXFileName="${QNXFileNamePrefix}-${QNXFileNameBoardName}-${QNXFileNameVariant}.bin"
}

SendWorkaroundFiles()
{
    :
}

#FIXME: Add instructions for DevAir
# called before nvflash is invoked
BoardPreflashSteps()
{
    echo "NOTE: All switches are on E1860/Jetson (base board)"
    echo "If board is ON, power OFF it by moving switch (S4) to OFF position."
    echo ""
    echo "Put the board in force recovery"
    echo "Press and hold the RECOVER button (S3) while switching POWER (S4) to ON"
    echo "Press Enter to continue"
    read
}


# called immediately after nvflash is done
BoardPostNvFlashSteps()
{
    :
}

#FIXME: Add instructions for DevAir
# called after the kernel was flashed with fastboot
BoardPostKernelFlashSteps()
{
    echo "To boot, reset the board by pressing the RESET button (S5) and releasing it"
}

# called if uboot was flashed
BoardUseUboot()
{
    :
}
