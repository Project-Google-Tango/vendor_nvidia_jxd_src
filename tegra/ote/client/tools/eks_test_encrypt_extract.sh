#!/bin/bash
#
# Copyright (c) 2013-2014 NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

# This script generates 8 random keys and encrypts then decrypts verifying
# correctness.

#no_cleanup=true

# generate_random_binary <output file> <size in bytes>
function generate_random_binary ()
{
	if [ $# -ne 2 ]; then
		echo "Usage: generate_random_binary <output file> <size>"
		return 1
	fi

	dd if=/dev/urandom of=${1} bs=${2} count=1 2>/dev/null
}

# bin2hex in out
function bin2hex ()
{
	if [ $# -ne 2 ]; then
		echo "Usage: bin2hex in out>"
		return 1
	fi

	# remove out if it already exists
	[ -f $2 ] && rm $2

	for b in `xxd -p -c 1 $1`; do
		echo -n "0x$b, " >> $2
	done
	sed 's/, $//' $2 > $2.tmp
	rm $2
	mv $2.tmp $2

}

generate_random_binary 0.a 312 #1024,    # HDCP KEY(1KB)
bin2hex 0.a 0.hex

generate_random_binary 1.a 8000  #8*1024,  # WMDRM CERT(8KB)
generate_random_binary 2.a 200   #256,     # WMDRM KEY(256B)
generate_random_binary 3.a 8000  #8*1024,  # PLAYREADY CERT(8KB)
generate_random_binary 4.a 200   #256,     # PLAYREADY KEY(256)
generate_random_binary 5.a 1000  #1024,    # WIDEVINE KEY(1024)
generate_random_binary 6.a 200   #256,     # NVSI(256)
generate_random_binary 7.a 862   #862      # HDCP2.x SINK KEY/CERT


python eks_encrypt_keys.py --sbk=0 \
	--hdcp_key=0.hex \
	--wmdrmpd_cert=1.a \
	--wmdrmpd_key=2.a \
	--prdy_cert=3.a \
	--prdy_key=4.a \
	--widevine_key=5.a \
	--nvsi_key=6.a \
	--hdcp2_sink_key=7.a \
        --algo_file=algo_file.dat \
        --perm_file=perm_file.dat \
	1>/dev/null

python eks_extract_key.py --eks=eks.dat --sbk=0 --index=0 --algo=aes-128-cbc --no_hex 1>/dev/null
python eks_extract_key.py --eks=eks.dat --sbk=0 --index=1 --algo=aes-128-cbc --no_hex 1>/dev/null
python eks_extract_key.py --eks=eks.dat --sbk=0 --index=2 --algo=aes-128-cbc --no_hex 1>/dev/null
python eks_extract_key.py --eks=eks.dat --sbk=0 --index=3 --algo=aes-128-cbc --no_hex 1>/dev/null
python eks_extract_key.py --eks=eks.dat --sbk=0 --index=4 --algo=aes-128-cbc --no_hex 1>/dev/null
python eks_extract_key.py --eks=eks.dat --sbk=0 --index=5 --algo=aes-128-cbc --no_hex 1>/dev/null
python eks_extract_key.py --eks=eks.dat --sbk=0 --index=6 --algo=aes-128-cbc --no_hex 1>/dev/null
python eks_extract_key.py --eks=eks.dat --sbk=0 --index=7 --algo=aes-128-cbc --no_hex 1>/dev/null

diffs=0
for x in `seq 0 7`; do
	if ! diff $x.a Key_$x.dat; then
		echo Slot $x does not match
		(( diffs += 1))
	fi
done

# cleanup
if [ ! "$no_cleanup" ]; then
	rm *.a Key_* eks.dat 0.hex
fi

if [ "$diffs" -gt 0 ]; then
	echo $diffs slots did not match!
	exit 1
else
	echo "SUCCESS: All keys matched"
fi

exit 0
