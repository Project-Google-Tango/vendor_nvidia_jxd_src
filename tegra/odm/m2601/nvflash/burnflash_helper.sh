#
# Copyright (c) 2012-2013, NVIDIA Corporation.  All rights reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

TargetType="m2601"
BoardKernArgs="usbcore.old_scheme_first=1"
BoardOptions="b:k:p:"
PartitionLayout=
Coherent_pool="coherent_pool=8M"
UserConfigFile=
ConfigBootMedium=0
QNXFileName="ifs-nvidia-tegra3-mmx2.bin"
ValidSkus=( 0 1 )
SkuValid=0
NotLegacy=1
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
    echo "-b : Selects boot device - NOR/IDE."
    echo "     Default value is quickboot_snor_linux.cfg for nor boot."
    echo "-p : Provide the partition Layout for the device in the mtdpart format."
    echo "     Partition start and size should be block size aligned"
    echo "     example: mtdparts:<DRIVER_NAME>:size@start(PART_NAME)"
    echo "            \"mtdparts=tegra-nor:39936K@0K(System),512000K@39936K,-(Efs) \""
    echo "            \"System partition should not be changed\""
    echo ""
    echo "Supported <NFSPORT>    : eth0 : On board USB-Ethernet Support(J11)"
    echo "                       : eth1 : USB-Ethernet adapter"
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

    for Sku in ${ValidSkus[@]}
    do
        if [ $SkuId -eq $Sku ]; then
            SkuValid=1
        fi
    done

    if [ $SkuValid -eq 0 ]; then
        echo "Invalid SKU id"
        AbnormalTermination
    fi

    if [ ${RcmMode} -eq 0 ] && [ ${FAMode} -eq 0 ];then
         if [ -z "$BootDevice" ]; then
              echo "Missing -b option (mandatory)"
              Usage
              AbnormalTermination
         fi


          if [ "$BootDevice" == "NOR" ]; then
                 BCT=${NvFlashDir}/M2601_Hynix_2GB_H5TQ4G83AFR_625MHz_120904_NOR.bct
          else
                if [ "$BootDevice" == "IDE" ]; then
                     BCT=${NvFlashDir}/M2601_Hynix_2GB_H5TQ4G83AFR_625MHz_120904_IDE.bct
                else
                     echo "Invalid Boot Device"
                     AbnormalTermination
                fi
           fi
    else
           if [ ${FAMode} -eq 1 ]; then
                BCT=${NvFlashDir}/M2601_Hynix_2GB_H5TQ4G83AFR_625MHz_120904_NOR_RCM_sku${SkuId}.bct
           fi
    fi

    let "ODMData = 0x80081105" # UART-A (Lower J7)

    if [ -z $ConfigFile ]; then
        if [ ! ${FlashQNX} -eq 1 ]; then
            if [ $Quickboot -eq 0 ] ; then
                ConfigFile=${NvFlashDir}/fastboot_snor_linux.cfg
            else
                if [ "$BootDevice" == "NOR" ]; then
                    ConfigFile=${NvFlashDir}/quickboot_snor_linux.cfg
                else
                    ConfigFile=${NvFlashDir}/quickboot_ide_linux.cfg
                fi
            fi
            if [ -z ${PartitionLayout} ]; then
                if [ $Quickboot -eq 0 ] ; then
                    PartitionLayout="mtdparts=tegra-nor:384K@17664K(env),47488K@18048K(userspace) "
                else
                    PartitionLayout="mtdparts=tegra-nor:39936K@0K(System),512000K@39936K,-(EFS) "
                fi
            fi
        else
            if [ $Quickboot -eq 0 ] ; then
                ConfigFile=${NvFlashDir}/fastboot_snor_qnx.cfg
            else
                ConfigFile=${NvFlashDir}/quickboot_snor_qnx.cfg
            fi
        fi
    fi
    BoardKernArgs="${BoardKernArgs} ${Coherent_pool} ${PartitionLayout}"
}

SendWorkaroundFiles()
{
    :
}

#FIXME: Add instructions for DevAir
# called before nvflash is invoked
BoardPreflashSteps()
{
    echo "NOTE: All switches are on E2602 (base board)"
    echo "If board is not ON, power it by moving switch S4  to ON position."
    echo "ON position depends on whether 12V DC supply OR ATX power supply"
    echo "is used to power the board. If the board is switched ON, RED LED (DS1) will glow."
    echo ""
    echo "Put the board in force recovery by putting the switch S1 to ON position"
    echo "(close to the S1 marking) and reset the board by pressing and releasing S2"
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
    echo "Move the recovery switch on OFF position (away from S1 marking)"
    echo "To boot, reset the board"
}

# called if uboot was flashed
BoardUseUboot()
{
    :
}
