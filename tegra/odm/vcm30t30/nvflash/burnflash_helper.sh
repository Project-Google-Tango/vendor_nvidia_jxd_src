# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

TargetType="vcm30t30"
BoardKernArgs="usbcore.old_scheme_first=1"
BoardOptions="k:p:m:Ev:"
PartitionLayout=
UserConfigFile=
MemorySize=
ConfigBootMedium=0
QNXFileNamePrefix="ifs-nvidia-tegra3"
QNXFileNameBoardName="vcm"
QNXFileNameVariant="profiler"
ValidSkus1858=( 2 )
ValidSkus1868=( 2 )
ValidBoards=( 1858 1868 )
SkuValid=0
BoardValid=0
Automation=0

#variable pointing to correct BCT. Format: BCT_${BoardId}_${SkuId}
BCT_1858_2='VCM3_Hynix_2GB_H5TQ4G83MFR_625MHz_110519_NOR.bct'
BCT_1868_2='VCM3_Hynix_2GB_H5TQ4G83MFR_625MHz_110519_NOR.bct'

#variable pointing to correct ODM data.
ODM_UARTD_2GB=0x80099105
ODM_UARTD_1GB=0x40099105
ODM_UARTB_2GB=0x80088105
ODM_UARTA_512MB=0x20081105
ODM_UARTB_512MB=0x20088105
#Format: ODM_${SkuId}_${BoardId}_${FlashQNX}
ODM_2_1858_1=ODM_UARTD_2GB
ODM_2_1868_1=ODM_UARTB_512MB

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
        E) Automation=1
           ;;
        p) PartitionLayout=$OPTARG
           ;;
        m) MemorySize=$OPTARG
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
    echo "-b : Provide board id from amongst the following: ${ValidBoards[@]}"
    echo "     This argument is not mandatory and defaults to 1858"
    echo "-S : Provide SKU id from amongst the following:"
    echo "     For Board 1858 - ${ValidSkus1858[@]}"
    echo "     For Board 1868 - ${ValidSkus1868[@]}"
    echo "-k : Provide the nvflash config file to be used."
    echo "     Default value is quickboot_snor_linux.cfg for nor boot."
    echo "-E : Selects Automation. Assumes board is in Recovery Mode and bypasses the \"Press ENTER to Continue\" message"
    echo "-p : Provide the partition Layout for the device in the mtdpart format."
    echo "     Partition start and size should be block size aligned"
    echo "     example: mtdparts:<DRIVER_NAME>:size@start(PART_NAME)"
    echo "              \"mtdparts=tegra-nor:384K@36992K(env),28160K@37376K(userspace) \""
    echo "-m : override the default memory size (512MB/1GB/2GB)"
    echo "-v : QNX image variant: profiler (default) or e1868"
    echo ""
    echo "Supported <NFSPORT>    : eth0 : Ethernet connector J58 on E1888 (baseboard)"
    echo "                       : (It is recommended not to connect a USB-Ethernet adapter during system boot, as the enumeration sequence is undeterministic)"
}

BoardValidateParams()
{
    # if SKU file is specified, extract sku id from the filename
    if [ -n "${ZFile}" ]; then
        SkuId=`echo $ZFile|cut -d '-' -f 3 | sed 's/0*//'`
        BoardId=`echo $ZFile|cut -d '-' -f 2 | cut -c 2-`
    fi

    if [ $SkuId -eq 0 ]; then
        echo "Missing -S option (mandatory)"
        Usage
        AbnormalTermination
    fi

    if [ $BoardId -eq 0 ]; then
        echo "Missing option -b. Defaulting board to 1858."
        BoardId=1858
    fi

    for Board in ${ValidBoards[@]}
    do
        if [ $BoardId -eq $Board ]; then
            BoardValid=1
        fi
    done

    if [ $BoardValid -eq 0 ]; then
        echo "Invalid Board id"
        AbnormalTermination
    fi


    if [ $BoardId -eq 1858 ]; then
        ValidSkus=("${ValidSkus1858[@]}")
    elif [ $BoardId -eq 1868 ]; then
        ValidSkus=("${ValidSkus1868[@]}")
    else
        ValidSkus=("${ValidSkus1858[@]}")
    fi

    for Sku in ${ValidSkus[@]}
    do
        if [ $SkuId -eq $Sku ]; then
            SkuValid=1
        fi
    done

    if [ $SkuValid -eq 0 ]; then
        echo "Invalid SKU id"
        echo "Valid Sku Ids for $BoardId are ${ValidSkus[@]}"
        AbnormalTermination
    fi

    if [[ "$QNXFileNameVariant" != "profiler" &&
          "$QNXFileNameVariant" != "e1868"
       ]]; then
        echo "Invalid QNX image variant" $QNXFileNameVariant
        AbnormalTermination
    fi

    eval BCT=${NvFlashDir}/'$'BCT_${BoardId}_${SkuId}
    let "ODMData = ODM_${SkuId}_${BoardId}_${FlashQNX}"

    if [ ! -z $MemorySize ]; then
        let "ODMData &= 0x1FFFFFFF"
        if [ "$MemorySize" = "512MB" ]; then
            let "ODMData |= 0x20000000"
        elif [ "$MemorySize" = "1GB" ]; then
            let "ODMData |= 0x40000000"
        elif [ "$MemorySize" = "2GB" ]; then
            let "ODMData |= 0x80000000"
        else
            echo "Invalid memory size" $MemorySize
            AbnormalTermination
        fi
    fi

    if [ -z $ConfigFile ]; then
        if [ ! ${FlashQNX} -eq 1 ]; then
            if [ $Quickboot -eq 0 ] ; then
                ConfigFile=${NvFlashDir}/fastboot_snor_linux.cfg
            else
                ConfigFile=${NvFlashDir}/quickboot_snor_linux.cfg
            fi
            if [ -z ${PartitionLayout} ]; then
                PartitionLayout="mtdparts=tegra-nor:384K@17664K(env),47488K@18048K(userspace) "
            fi
        else
            if [ $Quickboot -eq 0 ] ; then
                ConfigFile=${NvFlashDir}/fastboot_snor_qnx.cfg
            else
                ConfigFile=${NvFlashDir}/quickboot_snor_qnx.cfg
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

# called before nvflash is invoked
BoardPreflashSteps()
{
if [ $Automation -eq 0 ]; then
    echo "NOTE: All switches are on E1888 (base board)"
    echo "If board is not ON, power it by moving switch S3  to ON position."
    echo "ON position depends on whether 12V DC supply (J6) OR ATX power supply (J9)"
    echo "is used to power the board. If the board is switched ON, RED LED (DS2) will glow."
    echo ""
    echo "Put the board in force recovery by putting the switch S7 to ON position"
    echo "(close to the S7 marking) and reset the board by pressing and releasing S1"
    echo "Press Enter to continue"
    read
else
    echo "Assuming board is in Recovery Mode. Starting the Flashing Process"
fi
}


# called immediately after nvflash is done
BoardPostNvFlashSteps()
{
    :
}

# called after the kernel was flashed with fastboot
BoardPostKernelFlashSteps()
{
    echo "Move the recovery switch on OFF position (away from S7 marking)"
    echo "To boot, reset the board"
}

# called if uboot was flashed
BoardUseUboot()
{
    :
}
