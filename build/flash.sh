#!/bin/bash
#
# Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
#
# NVFlash wrapper script for flashing Android from either build environment
# or from a BuildBrain output.tgz package. This script is not intended to be
# called directly, but from vendorsetup.sh 'flash' function or BuildBrain
# package flashing script.

# Usage:
#  flash.sh [-n] [-b <file.bct>] [-c <file.cfg>] [-o <odmdata>] [-C <cmdline>]
#           [-s <skuid> [forcebypass]] -- [optional args]
#
# -C flag overrides the entire command line for nvflash, other three
# options are for explicitly specifying bct, cfg and odmdata options.
# -n skips using sudo on cmdline.
# optional arguments after '--' are added as-is to end of nvflash cmdline.

# Option precedence is as follows:
#
# 1. Command-line options (-b, -c, -o, -C) override all others.
#  (assuming there are alternative configurations to choose from:)
# 2. Shell environment variables (BOARD_IS_PM269, ENTERPRISE_A03 etc)
# 3. If shell is interactive, prompt for input from user
# 4. If shell is non-interactive, use default values

# Mandatory arguments, passed from calling scripts.
if [[ ! -d ${PRODUCT_OUT} ]]; then
    echo "error: \${PRODUCT_OUT} not set or not a directory"
    exit 1
fi

# Detect OS, then set/verify nvflash binary accordingly.
case $OSTYPE in
    cygwin)
        NVFLASH_BINARY="nvflash.exe"
        NVGETDTB_BINARY="nvgetdtb.exe"
        _nosudo=1
        ;;
    linux*)
        if [[ ! -x ${NVFLASH_BINARY} ]]; then
            echo "error: \${NVFLASH_BINARY} not set or not an executable file"
            exit 1
        fi
        if [[ ! -x ${NVGETDTB_BINARY} ]]; then
            echo "error: \${NVGETDTB_BINARY} not set or not an executable file"
            exit 1
        fi
        ;;
    *)
        echo "error: unsupported OS type $OSTYPE detected"
        exit 1
        ;;
esac

# Optional arguments
while getopts "nb:c:o:C:s:" OPTION
do
    case $OPTION in
    n) _nosudo=1;
        ;;
    b) _bctfile=${OPTARG};
        ;;
    c) _cfgfile=${OPTARG};
        ;;
    o) _odmdata=${OPTARG};
        ;;
    s) _skuid=${OPTARG};
        if [ "$3" == "forcebypass" ]; then
            _skuid="$_skuid $3"
            shift
        fi
        ;;
    C) shift ; _cmdline=$@;
        ;;
    esac
done

# Optional command-line arguments, added to nvflash cmdline as-is:
# flash -b my_flash.bct -- <args to nvflash>
shift $(($OPTIND - 1))
_args=$@

# Fetch target board name
product=$(echo ${PRODUCT_OUT%/} | grep -o '[a-zA-Z0-9]*$')

##################################
# tnspec
tnspec() {
    # return nothing if tnspec tool or spec file is missing
    if [[ ! -x $TNSPEC_BIN ]]; then
        echo "Error: tnspec.py (\"$TNSPEC_BIN\") doesn't exist or is not executable." >&2
        return
    fi
    if [[ ! -f $TNSPEC_SPEC ]]; then
        echo "Error: tnspec.json (\"$TNSPEC_SPEC\") doesn't exist." >&2
        return
    fi

    $TNSPEC_BIN $* -s $TNSPEC_SPEC
}

# Setup functions per target board
ardbeg() {
    odmdata=0x98000
    bctfile=bct.cfg
    skuid=auto

    tn_boards=$(tnspec list)

    if [[ -n $TNSPEC ]]; then
        if _in_array $TNSPEC $tn_boards; then
            echo "Selecting $TNSPEC"
            board=$TNSPEC
        else
            echo "error: Invalid TN board name : TNSPEC=$TNSPEC"
            echo "Supported TN board names:" $tn_boards
            exit 1
        fi
    fi

    if [[ -z $board ]] && _shell_is_interactive; then
        if [[ ! -z $tn_boards ]]; then
            echo "For tn8-* boards,"
            echo "Set \$TNSPEC to <board_name> to skip interactive mode."
            echo "'tnspec info <board_name>' to get more information about the board"
            echo ""
        fi

        # prompt user for target board info
        _choose "which board to flash?" "$tn_boards shield_ers laguna" board shield_ers
    else
        board=${board-shield_ers}
    fi

    # set bctfile and cfgfile based on target board
    if _in_array $board $tn_boards; then
        tnspec info $board

        cfgfile=$(tnspec cfg $board)
        [[ ${#cfgfile} == 0 ]] && unset cfgfile
        bctfile=$(tnspec bct $board)
        [[ ${#bctfile} == 0 ]] && unset bctfile
        dtbfile=$(tnspec dtb $board)
        [[ ${#dtbfile} == 0 ]] && unset dtbfile
        sku=$(tnspec sku $board)
        [[ ${#sku} > 0 ]] && skuid=$sku
        odm=$(tnspec odm $board)
        [[ ${#odm} > 0 ]] && odmdata=$odm

        # generate NCT
        tnspec nct $board > $PRODUCT_OUT/nct_$board.txt
        if [ $? -eq 0 ]; then
            nct="--nct nct_$board.txt"
        else
            echo "Failed to generate NCT file for $board"
        fi
    elif [[ $board == shield_ers ]]; then
        cfgfile=flash.cfg
    elif [[ $board == laguna ]]; then
        bctfile=flash_pm358_792.cfg
        cfgfile=laguna_flash.cfg
    fi
}

loki() {
    odmdata=0x69c000
    skuid=0x7

    # Set internal board identifier
    [[ -n $BOARD_IS_E2548 ]] && board=e2548_a02
    [[ -n $BOARD_IS_THOR_195 ]] && board=thor_195
    [[ -n $BOARD_IS_FOSTER_PRO ]] && board=foster_pro
    [[ -n $BOARD_IS_LOKI_NFF_B00 ]] && board=loki_nff_b00
    [[ -n $BOARD_IS_LOKI_FFD_PREM ]] && board=loki_ffd_prem
    [[ -n $BOARD_IS_LOKI_FFD_BASE ]] && board=loki_ffd_base
    [[ -n $BOARD_IS_LOKI_NFF_B00_2GB ]] && board=loki_nff_b00_2gb
    if [[ -z $board ]] && _shell_is_interactive; then
        # Prompt user for target board info
        _choose "Which board to flash?" "e2548_a02 loki_nff_b00 loki_nff_b00_2gb thor_195 loki_ffd_prem loki_ffd_prem_a01 loki_ffd_base foster_pro" board loki_nff_b00
    else
        board=${board-loki_nff_b00}
    fi

    # Set bctfile and cfgfile based on target board.
    # TEMP: always flash NCT for the boards until
    # final flashing procedure is fully implemented
    cfgfile=flash.cfg
    l_dtbfile=tegra124-loki.dtb
    if [[ $board == e2548_a02 ]]; then
        nct="--nct NCT_loki.txt"
        bctfile=bct.cfg
    elif [[ $board == loki_nff_b00 ]]; then
        nct="--nct NCT_loki_b00.txt"
        bctfile=bct_loki_b00.cfg
    elif [[ $board == foster_pro ]]; then
        nct="--nct NCT_foster.txt"
        l_dtbfile=tegra124-foster.dtb
        bctfile=bct_loki_ffd_sku0.cfg
        odmdata=0x29c000
    elif [[ $board == loki_ffd_prem ]]; then
        nct="--nct NCT_loki_ffd_sku0.txt"
        bctfile=bct_loki_ffd_sku0.cfg
    elif [[ $board == loki_ffd_prem_a01 ]]; then
        nct="--nct NCT_loki_ffd_sku0_a1.txt"
        bctfile=bct_loki_ffd_sku0.cfg
    elif [[ $board == loki_ffd_base ]]; then
        nct="--nct NCT_loki_ffd_sku100.txt"
        bctfile=bct_loki_ffd_sku100.cfg
    elif [[ $board == loki_nff_b00_2gb ]]; then
        nct="--nct NCT_loki_b00_sku100.txt"
        bctfile=bct_loki_b00_sku100.cfg
    elif [[ $board == thor_195 ]]; then
        nct="--nct NCT_thor1_95.txt"
        bctfile=bct_thor1_95.cfg
    fi
}

pluto() {
    odmdata=0x40098008
    bctfile=common_bct.cfg
}

roth() {
    odmdata=0x8049C000

    # set internal board identifier
    [[ -n $board_is_p2454 ]] && board=p2454
    if [[ -z $board ]] && _shell_is_interactive; then
        # prompt user for target board info
        _choose "which roth board revision to flash?" "p2560 p2454" board p2560
    else
        board=${board-p2454}
    fi

    # set bctfile and cfgfile based on target board
    if [[ $board == p2454 ]]; then
        bctfile=flash.bct
    elif [[ $board == p2560 ]]; then
        bctfile=flash_p2560_450Mhz.bct
        sif="--sysfile SIF.txt"
    fi
    bypass="--fusebypass_config fuse_bypass.txt --sku_to_bypass 0x4"
}

kai() {
    odmdata=0x40098000
}

dalmore() {
    # Set default ODM data
    odmdata=0x00098000
    bctfile=common_bct.cfg
}

macallan() {
    odmdata=0x00098000
}

cardhu() {
    # Set default ODM data
    odmdata=0x40080000

    # Set internal board identifier
    [[ -n $BOARD_IS_PM269 ]] && board=pm269
    [[ -n $BOARD_IS_PM305 ]] && board=pm305
    if [[ -z $board ]] && _shell_is_interactive; then
        # Prompt user for target board info
        _choose "Which board to flash?" "cardhu pm269 pm305" board cardhu
    else
        board=${board-cardhu}
    fi

    # Set bctfile and cfgfile based on target board
    if [[ $board == pm269 ]]; then
        bctfile=bct_pm269.cfg
    elif [[ $board == pm305 ]]; then
        bctfile=bct_pm305.cfg
    elif [[ $board == cardhu ]]; then
        bctfile=bct_cardhu.cfg
    fi
}

enterprise() {
    # Set internal board identifier
    [[ -n $ENTERPRISE_A01 ]] && board=a01
    [[ -n $ENTERPRISE_A03 ]] && board=a03
    [[ -n $ENTERPRISE_A04 ]] && board=a03
    if [[ -z $board ]] && _shell_is_interactive; then
        _choose "Which Enterprise board revision to flash?" "a01 a02 a03" board a02
    else
        board=${board-a02}
    fi

    # Set bctfile, cfgfile and odmdata based on target board
    if [[ $board == a01 ]]; then
        bctfile=bct_a01.cfg
        odmdata=0x3009A000
    elif [[ $board == a02 ]]; then
        bctfile=bct_a02.cfg
        odmdata=0x3009A000
    elif [[ $board == a03 ]]; then
        bctfile=flash_a03.cfg
        odmdata=0x4009A018
    fi
}

###################
# Utility functions

# Test if we have a connected output terminal
_shell_is_interactive() { tty -s ; return $? ; }

# Test if string ($1) is found in array ($2)
_in_array() {
    local hay needle=$1 ; shift
    for hay; do [[ $hay == $needle ]] && return 0 ; done
    return 1
}

# Display prompt and loop until valid input is given
_choose() {
    _shell_is_interactive || { "error: _choose needs an interactive shell" ; exit 2 ; }
    local query="$1"                   # $1: Prompt text
    local -a choices=($2)              # $2: Valid input values
    local input=$(eval "echo \${$3}")  # $3: Variable name to store result in
    local default=$4                   # $4: Default choice
    local selected=''
    while [[ -z $selected ]] ; do
        read -e -p "$query [${choices[*]}] " -i "$default" input
        if ! _in_array "$input" "${choices[@]}"; then
            echo "error: $input is not a valid choice. Valid choices are:"
            printf ' %s\n' ${choices[@]}
        else
            selected=$input
        fi
    done
    eval "$3=$selected"
    # If predefined input is invalid, return error
    _in_array "$selected" "${choices[@]}"
}

# Set all needed parameters
_set_cmdline() {
    # Set ODM data
    if [[ -z $odmdata ]] && [[ -z $_odmdata ]]; then
        echo "error: no ODM data found or provided for target product: $product"
        exit 1
    else
        odmdata=${_odmdata-${odmdata}}
    fi

    # Set BCT and CFG files (with fallback defaults)
    bctfile=${_bctfile-${bctfile-"bct.cfg"}}
    cfgfile=${_cfgfile-${cfgfile-"flash.cfg"}}
    bypass=${bypass-""}
    sif=${sif-""}
    nct=${nct-""}

    # set sku id only if it was previously intialized
    skuid=${_skuid-${skuid}}
    if [[ -n $skuid ]]; then
        skubypass="-s "$skuid
    fi

    # if fuse_bypass.txt is not present in out directory
    # then do not bypass sku.
    if [ ! -f $PRODUCT_OUT/fuse_bypass.txt ]; then
        skubypass=""
    fi

    # update dtb filename if not previously set
    if [[ -z $dtbfile ]]; then
        dtbfile=$(sudo $NVGETDTB_BINARY)
        if [ $? -eq 0 ]; then
             echo "INFO: nvgetdtb: Using $dtbfile for $product product"
        else
            echo "INFO: nvgetdtb couldn't retrieve the dtbfile for $product product"
            dtbfile=$(grep dtb ${PRODUCT_OUT}/$cfgfile | cut -d "=" -f 2)
            echo "INFO: Using the default product dtb file $l_dtbfile"
            dtbfile=$l_dtbfile
        fi
    fi

    # Parse nvflash commandline
    cmdline=(
        --bct $bctfile
        --setbct
        --odmdata $odmdata
        --configfile $cfgfile
        --dtbfile $dtbfile
        --create
        --bl bootloader.bin
        --wait
        $bypass
        $sif
        $skubypass
        $nct
        --go
    )
}

###########
# Main code

# If -C is set, override all others
if [[ $_cmdline ]]; then
    cmdline=(
        $_cmdline
    )
# If -b, -c and -o are set, use them
elif [[ $_bctfile ]] && [[ $_cfgfile ]] && [[ $_odmdata ]] && [[ $_skuid ]]; then
    _set_cmdline
else
    # Run product function to set needed parameters
    eval $product
    _set_cmdline
fi

# If -n is set, don't use sudo when calling nvflash
if [[ -n $_nosudo ]]; then
    cmdline=($NVFLASH_BINARY ${cmdline[@]})
else
    cmdline=(sudo $NVFLASH_BINARY ${cmdline[@]})
fi

if [[ $_args ]]; then
    # This assumes '--go' is last in cmdline
    unset cmdline[${#cmdline[@]}-1]
    cmdline=(${cmdline[@]} ${_args[@]} --go)
fi

echo "INFO: PRODUCT_OUT = $PRODUCT_OUT"
echo "INFO: CMDLINE = ${cmdline[@]}"

# Execute command
(cd $PRODUCT_OUT && eval ${cmdline[@]})
exit $?
