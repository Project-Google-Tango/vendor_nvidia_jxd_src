#
# Copyright (c) 2013-2014 NVIDIA CORPORATION.  All Rights Reserved.
#
# NVIDIA CORPORATION and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA CORPORATION is strictly prohibited.

from common import *
from struct import *
from optparse import OptionParser
import sys
import os
import pdb
import string
import shutil
import struct
import zlib

#------------------------------------------
# Version: 2
#------------------------------------------
# This is version 2 of the file.
# Version 1 does not have support for algo and permission files


#------------------------------------------
# OpenSSL configuration  :
#------------------------------------------
# Open SSL Path
# Please Choose here either use the openssl tool in the product package or your
# local installed openssl # For the second case, please uncomment the second
# line, replace the example path with
#    hard-coded openssl executable path and then comment the first line.
#openssl =   FindOpenSSL()
#openssl =  "~/system/bin/openssl"
openssl =  "/usr/bin/openssl"

#------------------------------------------
# Globals used by the script
#------------------------------------------
# A well known buffer will be encoded with the SBK key.
# DO NOT modify the value of these buffers !!
SBK_temp_buffer = "59e2636124674ccd840cb42c8e84f116"
iv = "00000000000000000000000000000000"

gOptions = ""

algo_dict = {
        1 : 'aes-128-cbc',
        3 : 'aes-128-ecb',
        }
#------------------------------------------
# internal functions
#------------------------------------------

def ParseCmd():
    global gOptions

    # Parse the command line
    parser = OptionParser()
    parser.add_option("--eks", dest="eksFile", type="string", \
           help="Encrypted Key Storage file: \
           full path of the file which has encrypted keys")
    parser.add_option("--sbk", dest="sbk", type="string", \
           help="SBK key value")
    parser.add_option("--index", dest="keyIndex", type="string", \
           help="Key Slot Index: slot index of the key to be extracted")
    parser.add_option("--algo", dest="algo", type="string", \
           help="algorithms supported by openssl")
    parser.add_option("--no_hex", dest="no_hex", action="store_true", \
           help="Do not decode keyslot as a hex encoded key.")

    (gOptions, args) = parser.parse_args()

    # SBK key to encrypt, should not be void
    if gOptions.sbk == None or gOptions.eksFile == None \
            or gOptions.keyIndex == None or gOptions.algo == None:
        print "Error -> ParseCmd: missing argument(s)."
        parser.print_help(file = sys.stderr)
        exit(1)

    if not os.path.isfile(gOptions.eksFile):
        print "Error -> ParseCmd: EKS file doesn't exist"
        print "        " + gOptions.eksFile
        parser.print_help(file = sys.stderr)
        exit(1)

def ExtractKey():
    global gOptions
    global SBK_temp_buffer
    global iv
    global openssl

    version2Flag = False
    print "Generate TF_SBK."

    # Derive a key from SBK and encrypt data with aes-cbc
    hTempBuffer = open ("temp_buffer.txt",'w')
    hTempBuffer.write (SBK_temp_buffer.decode('hex'))
    hTempBuffer.close()
    key     = Exec([openssl, 'aes-128-cbc', '-in', \
            'temp_buffer.txt', '-K', gOptions.sbk, '-iv', iv])
    os.remove("temp_buffer.txt")

    hEksFile = open ( gOptions.eksFile,'rb')

    data = hEksFile.read(4)
    magicID = hEksFile.read(8)

    length = struct.unpack("i", data)[0]

    tmp = 'NVEKSP\0\0'
    if magicID != tmp:
        print "Error -> Invalid Partition File"
        exit(1)

    crcLength = length - 4
    hEksFile.seek(4)
    data = hEksFile.read(crcLength);
    crcData = hEksFile.read(4);

    crc = zlib.crc32(data)
    crcEks = struct.unpack("i", crcData)[0]

    if crc != crcEks:
        print "Error -> Invalid Partition File"
        exit(1)

    hEksFile.seek(12)
    # Retrieve numKeys
    data = hEksFile.read(4)
    numOfKeys = struct.unpack("i", data)[0]

    index = int(gOptions.keyIndex)
    if (index > numOfKeys - 1):
        print "Error -> Invalid Key Index"
        exit(1)

    for i in range (0,index+1):
        data = hEksFile.read(4)
        keyLength = struct.unpack("i", data)[0]
        if (i == index):
            if (keyLength == 0):
                print "The key slot has NO(zero-length) key"
                exit(1)

            data = hEksFile.read(keyLength)
            count = 0
            for j in range(len(data)-1,-1,-1):
                if (data[j] == '\0'):
                    count = count + 1
                else:
                    break

            keyData = data[:len(data)-count]
            if not gOptions.no_hex:
                keyData = keyData.decode('hex')

            hTmpFile = open ( "tmpKey.dat", "wb" )
            hTmpFile.write(keyData)
            hTmpFile.close()

            outputKeyslot = Exec([openssl, gOptions.algo, '-d', '-in', \
                    'tmpKey.dat', '-K', key.encode('hex'), '-iv', iv])
            os.remove("tmpKey.dat")
            length = len(outputKeyslot)

            hTmpFile = open ("tmpKey.dat", "w+")
            hTmpFile.write(outputKeyslot)
            hTmpFile.close()

            hTmpFile = open ("tmpKey.dat", "r")
            data = hTmpFile.read(4)
            algo = struct.unpack("i", data)[0]

            data = hTmpFile.read(16)
            hPermFile = open ("permissions_file.dat", "wb")
            hPermFile.write(data)
            hPermFile.close()

            hEncKey = open ("enc_key.dat", "wb")
            data = hTmpFile.read(length - 20)
            hEncKey.write(data)
            hEncKey.close()
            outputKey = Exec([openssl, algo_dict[algo], '-d', '-in', \
                    'enc_key.dat', '-K', key.encode('hex'), '-iv', iv])

            hKeyFile = open ( "Key_" + str(index) + ".dat", "wb" )
            hKeyFile.write(outputKey)
            hKeyFile.close()

            os.remove("tmpKey.dat")
            os.remove("enc_key.dat")
        else:
            hEksFile.seek(keyLength, os.SEEK_CUR)

    hEksFile.close()

#------------------------------------------
# Main function, script entry point
#------------------------------------------
def main():
    global gOptions
    print ""
    ParseCmd()
    ExtractKey()

    print "Extracted the key at " + gOptions.keyIndex + " to Key_" \
            + gOptions.keyIndex + ".dat"
main()
