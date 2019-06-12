#!/bin/bash

#
# nvdumper_unit.sh
#
# Run some unit tests on nvdumper
#

die() {
    echo $@
    exit 1
}

fail() {
    line="${1}"
    shift
    echo -n "failed on line ${line} "
    echo $@
    exit 1
}

cleanup() {
    rm -rf "${TDIR}"
}

[ $# -ne 1 ] && die "nvdumper_unit.sh: you must supply an argument, \
the path to nvdumper"
NVDUMPER=$1
"${NVDUMPER}" -h &> /dev/null \
    || die "nvdumper_unit.sh: failed to run ${NVDUMPER}"
TDIR="`mktemp -d`"
trap cleanup INT TERM EXIT
# take a small file for a round trip
echo -n foof > "${TDIR}/foof" || fail "${LINENO}"
"${NVDUMPER}" -a "write" "${TDIR}/foof.dum" < "${TDIR}/foof" || fail "${LINENO}"
"${NVDUMPER}" -a "verify" "${TDIR}/foof.dum" || fail "${LINENO}"
"${NVDUMPER}" -a "read" "${TDIR}/foof.dum" > "${TDIR}/foof.again" || fail "${LINENO}"
cmp "${TDIR}/foof" "${TDIR}/foof.again" || fail "${LINENO}"
# test with verbose flag
"${NVDUMPER}" -v -a "write" "${TDIR}/foof.dum" < "${TDIR}/foof" 2> /dev/null || fail "${LINENO}"
"${NVDUMPER}" -v -a "verify" "${TDIR}/foof.dum" 2> /dev/null || fail "${LINENO}"
"${NVDUMPER}" -v -a "read" "${TDIR}/foof.dum" 2> /dev/null > "${TDIR}/foof.again" || fail "${LINENO}"
cmp "${TDIR}/foof" "${TDIR}/foof.again" || fail "${LINENO}"
# test an empty file
touch "${TDIR}/empty"
"${NVDUMPER}" -a "write" "${TDIR}/empty.dum" < "${TDIR}/empty" || fail "${LINENO}"
"${NVDUMPER}" -a "verify" "${TDIR}/empty.dum" || fail "${LINENO}"
"${NVDUMPER}" -a "read" "${TDIR}/empty.dum" > "${TDIR}/empty.again" || fail "${LINENO}"
cmp "${TDIR}/empty" "${TDIR}/empty.again" || fail "${LINENO}"
# Test with a binary file with high bits set
echo "0000000: 0358 b4e7                                .X.." \
        > "${TDIR}/bin.txt" || fail "${LINENO}"
xxd -r "${TDIR}/bin.txt" > "${TDIR}/bin" || fail "${LINENO}"
"${NVDUMPER}" -a "write" "${TDIR}/bin.dum" < "${TDIR}/bin" || fail "${LINENO}"
"${NVDUMPER}" -a "verify" "${TDIR}/bin.dum" || fail "${LINENO}"
"${NVDUMPER}" -a "read" "${TDIR}/bin.dum" > "${TDIR}/bin.again" || fail "${LINENO}"
cmp "${TDIR}/bin" "${TDIR}/bin.again" || fail "${LINENO}"
# (small) test of write padding a file out to a multiple of 4
echo -n "a" > "${TDIR}/a" || fail "${LINENO}"
"${NVDUMPER}" -a "write" "${TDIR}/a.dum" < "${TDIR}/a" || fail "${LINENO}"
"${NVDUMPER}" -a "verify" "${TDIR}/a.dum" || fail "${LINENO}"
dd if=/dev/zero of="${TDIR}/a" oflag=append conv=notrunc \
        bs=1 count=3 &> /dev/null || fail "${LINENO}"
"${NVDUMPER}" -a "read" "${TDIR}/a.dum" > "${TDIR}/a.again" || fail "${LINENO}"
cmp "${TDIR}/a" "${TDIR}/a.again" || fail "${LINENO}"
# test write padding a file out to a multiple of 4
head -c 4194301 /dev/urandom > "${TDIR}/uran" || fail "${LINENO}"
"${NVDUMPER}" -a "write" "${TDIR}/uran.dum" < "${TDIR}/uran" || fail "${LINENO}"
"${NVDUMPER}" -a "verify" "${TDIR}/uran.dum" || fail "${LINENO}"
dd if=/dev/zero of="${TDIR}/uran" oflag=append conv=notrunc \
	bs=1 count=3 &> /dev/null || fail "${LINENO}"
"${NVDUMPER}" -a "read" "${TDIR}/uran.dum" > "${TDIR}/uran.again" || fail "${LINENO}"
cmp "${TDIR}/uran" "${TDIR}/uran.again" || fail "${LINENO}"
# test a big file
dd if=/dev/urandom of="${TDIR}/big" bs=16384 count=65536 &> /dev/null \
	|| fail "${LINENO}" ": failed to create 1 GB file"
"${NVDUMPER}" -a "write" "${TDIR}/big.dum" < "${TDIR}/big" || fail "${LINENO}"
"${NVDUMPER}" -a "verify" "${TDIR}/big.dum" || fail "${LINENO}"
"${NVDUMPER}" -a "read" "${TDIR}/big.dum" > "${TDIR}/big.again" || fail "${LINENO}"
cmp "${TDIR}/big" "${TDIR}/big.again" || fail "${LINENO}"

exit 0
