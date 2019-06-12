#!/bin/bash

# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#  * Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#  * Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#  * Neither the name of NVIDIA CORPORATION nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

usage ()
{
cat << EOF
Usage: sudo ./nvsecureflow.sh [options]
    options:
    -h----------------print this message.
    -b----------------nvflash BCT file. By default flash.bct.
    -c----------------nvflash config file. By default flash.cfg.
    -B----------------bootloader filename. By default bootloader.bin.
    -o----------------ODM data. No default value for it.
    -m----------------booting mode can be sbk or pkc. By default pkc.
    -i----------------chip id. By default 0x35.
    -f----------------fuse config file. By default fuse_write.txt.
    -p----------------pkc rsa priv file. By default rsa_priv.txt.
    -k----------------keypair generation file. By default rsa_priv.txt.
    -d----------------d_session key filename. By default d_session.txt.
    -u----------------public key filename. By default spub.txt.
    -r----------------private key filename. By default spriv.txt.
    -e----------------oem filename. No default value.
    -x----------------binary path for nvflash & nvsecuretool. No default value.
EOF
	exit $1;
}

zflag=1;

while getopts "h:b:c:B:o:m:i:f:k:p:d:u:r:e:x:z" OPTION
do
	case $OPTION in
	h) usage 0;
	   ;;
	b) bct=${OPTARG};
	   ;;
	c) cfg=${OPTARG};
	   ;;
	B) bootloader=${OPTARG};
	   ;;
	o) odmdata=${OPTARG};
	   ;;
	m) mode=${OPTARG};
	   ;;
	i) chip=${OPTARG};
	   ;;
	f) fuse=${OPTARG};
	   ;;
	k) rsa_key=${OPTARG};
	   ;;
	p) rsa_flash=${OPTARG};
	   ;;
	d) d_session=${OPTARG};
	   ;;
	u) spub=${OPTARG};
	   ;;
	r) spriv=${OPTARG};
	   ;;
	e) oem=${OPTARG};
	   ;;
	x) path=${OPTARG};
           ;;
	z) zflag=0;
	   ;;
	?) usage 1;
	   ;;
	esac
	zflag=0;
done

if [ $zflag -ne 0 ]; then
	usage 1;
fi

# Validate OdmData
if [ -z "${odmdata}" ]; then
	echo "Invalid odmdata"
	exit 1;
fi

if [ -z "${path}" ]; then
        echo ${path}
	echo "nvflash, nvsecuretool binary path mising"
	exit 1;
fi

if [ -z "${mode}" ]; then
	mode=pkc
fi

shopt -s nocasematch
case "$mode" in
        "pkc" )

        # rsa_priv file for flashing
        if [ -z "${rsa_flash}" ]; then
                rsa_flash=rsa_priv.txt
                echo -e "taking rsa_priv.txt for flashing\n"
        fi;;

        "sbk" )
        # Enter sbk key
        read -p "Enter secure boot key: " sbk
        if [ -z "${sbk}" ]; then
                echo -e "sbk is missing\n"
                exit 1;
        fi;;
esac

# rsa_priv file for generating keypairs
if [ -z "${rsa_key}" ]; then
	rsa_key=rsa_priv.txt
fi

# public_key output filename
if [ -z "${spub}" ]; then
	spub=spub.txt
fi

# private_key output filename
if [ -z "${spriv}" ]; then
	spriv=spriv.txt
fi

# bct filename
if [ -z "${bct}" ]; then
	bct=flash.bct
fi

# flashing cfg
if [ -z "${cfg}" ]; then
	cfg=flash.cfg
fi

# bootloader filename
if [ -z "${bootloader}" ]; then
	bootloader=bootloader.bin
fi

# chipid
if [ -z "${chip}" ]; then
	chip=0x35
fi

# fuse_cfg
if [ -z "${fuse}" ]; then
	fuse=fuse_write.txt
fi

# d_session filename
if [ -z "${d_session}" ]; then
	d_session=d_session.txt
fi

echo -e "####Generating key pairs####\n"
sudo ${path}/nvsecuretool --keygen ${rsa_key} --keyfile ${spub} ${spriv}
status=$?
if [ $status -ne 0 ]; then
	echo -e "\n####Key pair generation failure####\n"
	exit 1;
fi

echo -e "\n####Generating d_session keys####\n"
sudo ${path}/nvflash --bct ${bct} --setbct --odmdata ${odmdata} --configfile ${cfg} --symkeygen ${spub} ${d_session} --bl ${bootloader}
status=$?
if [ $status -ne 0 ]; then
	echo -e "\n####D_session generation failure####\n"
	exit 1;
fi

echo -e "\n####Encrypting secure payload####\n"
shopt -s nocasematch
case "$mode" in
        "pkc" )
	cmdline=(sudo ${path}/nvsecuretool --pkc ${rsa_flash} --chip ${chip} --cfg ${cfg} --bct ${bct} --bl ${bootloader} --dsession ${d_session} --fuse_cfg ${fuse} --keyfile ${spub} ${s_priv})
	if [ -n "${oem}" ]; then
		cmdline+=(--oemfile ${oem})
	fi
	eval ${cmdline[@]}
	status=$?
	if [ $status -ne 0 ]; then
		echo -e "\n####Secure payload encryption failure####\n"
		exit 1;
	fi;;
        "sbk" )
        cmdline=(sudo ${path}/nvsecuretool --sbk ${sbk} --chip ${chip} --cfg ${cfg} --bct ${bct} --bl ${bootloader} --dsession ${d_session} --fuse_cfg ${fuse} --keyfile ${spub} ${s_priv})
        if [ -n "${oem}" ]; then
                cmdline+=(--oemfile ${oem})
        fi
        eval ${cmdline[@]}
        status=$?
        if [ $status -ne 0 ]; then
                echo -e "\n####Secure payload encryption failure####\n"
                exit 1;
	fi;;
esac

echo -e "\n####Flashing the device####\n"
sudo ${path}/nvflash --resume --bct ${bct} --setbct --odmdata ${odmdata} --configfile ${cfg} --create
status=$?
if [ $status -ne 0 ]; then
	echo -e "\n##Flashing failure####\n"
	exit 1;
fi

echo -e "\n####Burning the fuses####\n"
sudo ${path}/nvflash --resume --d_fuseburn blob.bin --go
status=$?
if [ $status -ne 0 ]; then
	echo -e "\n####Fuse burning failure####\n"
	exit 1;
fi
