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
# local installed openssl
# For the second case, please uncomment the second line, replace the example
# path with hard-coded openssl executable path and then comment the first line.
#openssl =   FindOpenSSL()
#openssl =  "~/system/bin/openssl"
openssl =  "/usr/bin/openssl"

#------------------------------------------
# Slots management
#------------------------------------------
# List of slots sizes (One size per slot : 1k for pattern deadbee0, 8k for
# pattern deadbee1...)
#
# Current encrypted key slots and sizes
gSSlotsSizes = [
	1024,    # HDCP KEY(1KB)
	8*1024,  # WMDRM CERT(8KB)
	256,     # WMDRM KEY(256B)
	8*1024,  # PLAYREADY CERT(8KB)
	256,     # PLAYREADY KEY(256)
	1024,    # WIDEVINE KEY(1024)
	256,     # NVSI(256)
	864,     # HDCP2.x SINK KEY/CERT
	]

# The maximal size that the generated key and certificate can occupy
gKeyMaxSize = gSSlotsSizes[2]
gCertMaxSize = gSSlotsSizes[1]
gHDCPMaxSize = gSSlotsSizes[0]
gWidevineMaxSize = gSSlotsSizes[5]
gNVSIMaxSize = gSSlotsSizes[6]
gHDCP2MaxSize = gSSlotsSizes[7]

#------------------------------------------
# Globals used by the script
#------------------------------------------
# A well known buffer will be encoded with the SBK key.
# DO NOT modify the value of these buffers !!
SBK_temp_buffer = "59e2636124674ccd840cb42c8e84f116"
iv = "00000000000000000000000000000000"

gOptions = ""
gNumKeys = len(gSSlotsSizes)
gWorkingHDCPFile = "HDCP_key.dat"

encrypted_wcert = ""
encrypted_wkey = ""
encrypted_cert = ""
encrypted_key = ""
encrypted_HDCP_key = ""
encrypted_widevine_key = ""
encrypted_nvsi_key = ""
encrypted_hdcp2_sink_key = ""
uuid_size = 16
algo_size = 1

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
    parser.add_option("--sbk", dest="sbk", type="string", \
           help="SBK key value")
    parser.add_option("--wmdrmpd_key", dest="wmdrmpd_key", type="string", \
           help="<wmdrmpd_key>: mandatory argument, the full path of wmdrm key")
    parser.add_option("--wmdrmpd_cert", dest="wmdrmpd_cert", type="string", \
           help="<wmdrmpd_cert>: mandatory argument, \
           full path of wmdrm certificate")
    parser.add_option("--prdy_key", dest="prdy_key", type="string", \
           help="<prdy_key>: mandatory argument, full path of playready key")
    parser.add_option("--prdy_cert", dest="prdy_cert", type="string", \
           help="<prdy_cert>: mandatory argument, \
           full path of playready certificate")
    parser.add_option("--hdcp_key", dest="hdcp_key", type="string", \
           help="<hdcp_key>: optional argument, file that contains the key for the HDCP service.")
    parser.add_option("--hdcp2_sink_key", dest="hdcp2_sink_key", type="string", \
           help="<hdcp2_sink_key>: optional argument, file that contains the sink key/cert for the HDCP2.x service.")
    parser.add_option("--widevine_key", dest="widevine_key", type="string", \
           help="<widevine_key>: optional argument, \
           file that contains the key for the Widevine service.")
    parser.add_option("--nvsi_key", dest="nvsi_key", type="string", \
           help="<nvsi_key>: optional argument, \
           file that contains the key for the NVSI key.")
    parser.add_option("--algo_file", dest="algo_file", type="string", \
           help="<algo_file>: mandatory argument, \
           file that contains the algorithm to be used for encryption")
    parser.add_option("--perm_file", dest="perm_file", type="string", \
           help="<permissions_file>: mandatory argument, \
           file that contains the user IDs which have required permissions.")


    (gOptions, args) = parser.parse_args()

    # SBK key to encrypt, should not be void
    if gOptions.sbk == None:
        print "Error -> ParseCmd: the SBK is a mandatory argument, \
                should not be void."
        parser.print_help(file = sys.stderr)
        exit(1)

    # Input path of the algo's file should not be void
    if gOptions.algo_file == None:
        print "Error -> ParseCmd: the algo file is a mandatory argument. \
                Should not be void. "
        parser.print_help(file = sys.stderr)
        exit(1)

    if gOptions.perm_file == None:
        print "Error -> ParseCmd: the permission file is a mandatory argument. \
                Should not be void. "
        parser.print_help(file = sys.stderr)
        exit(1)

    if gOptions.algo_file != None:
        if gOptions.algo_file.strip() == None:
            print "Error -> ParseCmd: the path of the algo_file \
                    should not be void."
            parser.print_help(file = sys.stderr)
            exit(1)
        elif not os.path.isfile(gOptions.algo_file):
            print "Error -> ParseCmd: the indicated algo file doesn't exist."
            parser.print_help(file = sys.stderr)
            exit(1)

    # Input path of the permissions file should not be void
    if gOptions.perm_file != None:
        if gOptions.perm_file.strip() == None:
            print "Error -> ParseCmd: the path of the permissions file \
                    should not be void."
            parser.print_help(file = sys.stderr)
            exit(1)
        elif not os.path.isfile(gOptions.perm_file):
            print "Error -> ParseCmd: the indicated permissions\
                    file doesn't exist."
            parser.print_help(file = sys.stderr)
            exit(1)

    # Input path of wmdrm key should not be void
    if gOptions.wmdrmpd_key != None or gOptions.wmdrmpd_cert != None \
       or gOptions.prdy_key != None or gOptions.prdy_cert != None:
        if gOptions.wmdrmpd_key == None or gOptions.wmdrmpd_key.strip() == None:
            print "Error -> ParseCmd: the path of wmdrm key should not be void."
            parser.print_help(file = sys.stderr)
            exit(1)
        elif not os.path.isfile(gOptions.wmdrmpd_key):
            print "Error -> ParseCmd: the indicated wmdrm key does't exist:"
            print "        " + gOptions.wmdrmpd_key
            parser.print_help(file = sys.stderr)
            exit(1)
        # Input path of wmdrm certificate should not be void
        if gOptions.wmdrmpd_cert == None or \
                gOptions.wmdrmpd_cert.strip() == None:
            print "Error -> ParseCmd: the path of wmdrm certificate \
                    should not be void."
            exit(1)
        elif not os.path.isfile(gOptions.wmdrmpd_cert):
            print "Error -> ParseCmd: the indicated wmdrm certificate \
                    does't exist:"
            print "        " + gOptions.wmdrmpd_cert
            parser.print_help(file = sys.stderr)
            exit(1)
        # Input path of playready key should not be void
        if gOptions.prdy_key == None or gOptions.prdy_key.strip() == None:
            print "Error -> ParseCmd: the path of play ready key \
                    should not be void."
            parser.print_help(file = sys.stderr)
            exit(1)
        elif not os.path.isfile(gOptions.prdy_key):
            print "Error -> ParseCmd: the indicated play ready key \
                    does't exist:"
            print "        " + gOptions.prdy_key
            parser.print_help(file = sys.stderr)
            exit(1)
        # Input path of model certificate should not be void
        if gOptions.prdy_cert == None or gOptions.prdy_cert.strip() == None:
            print "Error -> ParseCmd: the path of prdy cert should not be void."
            parser.print_help(file = sys.stderr)
            exit(1)
        elif not os.path.isfile(gOptions.prdy_cert):
            print "Error -> ParseCmd: the indicated play ready certificate \
                    does't exist:"
            print "        " + gOptions.prdy_cert
            parser.print_help(file = sys.stderr)
            exit(1)

    # Input path of HDCP should not be void
    if gOptions.hdcp_key != None:
        if not os.path.isfile(gOptions.hdcp_key):
            print "Error -> ParseCmd: the indicated file for \
                    HDCP key does't exist:"
            print "        " + gOptions.hdcp_key
            parser.print_help(file = sys.stderr)
            exit(1)

        # Convert the HDCP key structure in a binary file
        hdcp_key_txt = open(gOptions.hdcp_key, "r")
        hdcp_key_txt_buff = hdcp_key_txt.read()
        hdcp_key_txt_list = hdcp_key_txt_buff.split(", ")
        # Check the private key length is 312 bytes
        if len(hdcp_key_txt_list) != 312:
            print "Error -> The HDCP private key should be 312 bytes length and the format must be '0x00, 0x00 ...'"
            exit(1)

        # Convert to binary
        hdcp_key_bin_buff = ""
        for i in range(312):
            hdcp_key_bin_buff = hdcp_key_bin_buff + \
                    pack('B', int(hdcp_key_txt_list[i], 16))
        hdcp_key_txt.close()
        hdcp_key_bin = open(gWorkingHDCPFile, "wb")
        hdcp_key_bin.write(hdcp_key_bin_buff)
        hdcp_key_bin.close()

    if gOptions.hdcp2_sink_key != None:
        if not os.path.isfile(gOptions.hdcp2_sink_key):
            print "Error -> ParseCmd: the indicated file for HDCP2.x key/cert does't exist:"
            print "        " + gOptions.hdcp2_sink_key
            parser.print_help(file = sys.stderr)
            exit(1)

    # Input path of widevine should not be void
    if gOptions.widevine_key != None:
        if gOptions.widevine_key == None or \
                gOptions.widevine_key.strip() == None:
            print "Error -> ParseCmd: the path of \
                    widevine key should not be void."
            parser.print_help(file = sys.stderr)
            exit(1)
        elif not os.path.isfile(gOptions.widevine_key):
            print "Error -> ParseCmd: the indicated widevine key does't exist:"
            print "        " + gOptions.widevine_key
            parser.print_help(file = sys.stderr)
            exit(1)

    # Input path of nvsi should not be void
    if gOptions.nvsi_key != None:
        if gOptions.nvsi_key == None or gOptions.nvsi_key.strip() == None:
            print "Error -> ParseCmd: the path of nvsi key should not be void."
            parser.print_help(file = sys.stderr)
            exit(1)
        elif not os.path.isfile(gOptions.nvsi_key):
            print "Error -> ParseCmd: the indicated nvsi key doesn't exist:"
            print "        " + gOptions.nvsi_key
            parser.print_help(file = sys.stderr)
            exit(1)

def GetEncryptedSBK():
    global gOptions
    global gNumKeys
    global gWorkingHDCPFile
    global encrypted_wcert
    global encrypted_wkey
    global encrypted_cert
    global encrypted_key
    global encrypted_HDCP_key
    global encrypted_widevine_key
    global encrypted_nvsi_key
    global encrypted_hdcp2_sink_key
    global gKeyMaxSize
    global gCertMaxSize
    global gHDCPMaxSize
    global SBK_temp_buffer
    global iv
    global openssl
    global uuid_size

    print "Generate TF_SBK."

    # Derive a key from SBK and encrypt data with aes-cbc
    hTempBuffer = open ("temp_buffer.txt",'w')
    hTempBuffer.write (SBK_temp_buffer.decode('hex'))
    hTempBuffer.close()
    key     = Exec([openssl, 'aes-128-cbc', '-in', 'temp_buffer.txt', \
            '-K', gOptions.sbk, '-iv', iv])
    os.remove("temp_buffer.txt")

    hEncDataFile = open ("eks_temp.dat",'a+b')

    # MAGIC NUM
    hEncDataFile.write('NVEKSP')
    # should be 'h' because the length of magic id is 8 including 'null'
    format = "h"
    # null terminated
    data = struct.pack(format, 0)
    hEncDataFile.write(data)

    format = "i"
    # Open the algorithm and permission files
    algofile = open(gOptions.algo_file, 'r')
    ipermfile = open("intern_perm_file.dat", 'w+')

    for line in open(gOptions.perm_file):  # opened in text-mode; all EOLs are converted to '\n'
        line = line.rstrip('\n')
        ipermfile.write(line)
    ipermfile.close()

    tpermfile = open("intern_perm_file.dat", 'r')
    permfile = open("temp_perm_file.dat", 'w+')

    while True:
        data = tpermfile.read(2)
        if len(data) != 2:
            break

        # Converting to the required binary form
        permfile.write(chr(int(data,16)))

    permfile.close()
    os.remove("intern_perm_file.dat")

    # number of keys
    data = struct.pack(format, gNumKeys)
    hEncDataFile.write(data)

    cformat = "b"
    permfile = open("temp_perm_file.dat", 'r')

    ##############################
    #### Encrypt the key data ####
    ##############################

    print "Encrypt data with TF_SBK."
    if gOptions.hdcp_key != None:

        tmpfile = open("permalgofile.dat", 'w+')
        data = algofile.read(algo_size)
        algo = int(data)
        data = struct.pack(format, algo)
        tmpfile.write(data)
        perm = permfile.read(uuid_size)
        tmpfile.write(perm)

        encrypted_HDCP_key = Exec([openssl, algo_dict[algo], '-in', \
                gWorkingHDCPFile, '-K', key.encode('hex'), '-iv', iv])
        if len(encrypted_HDCP_key) > gHDCPMaxSize:
            print "Error -> GetEncryptedSBk: encrypted HDCP key exceeds "+ \
                    str(gHDCPMaxSize)+"."
            exit(1)

        tmpfile.write(encrypted_HDCP_key)
        tmpfile.close()
        keyslot = Exec([openssl, 'aes-128-cbc', '-in', 'permalgofile.dat',\
                '-K', key.encode('hex'), '-iv', iv])

        length = len(keyslot)
        data = struct.pack(format, length)
        hEncDataFile.write(data)
        hEncDataFile.write(keyslot)

        os.remove('permalgofile.dat')
    else:
        algofile.read(algo_size)
        permfile.read(uuid_size)
        data = struct.pack(format, 0)
        hEncDataFile.write(data)

    if gOptions.wmdrmpd_cert != None:

        tmpfile = open("permalgofile.dat", 'w+')
        data = algofile.read(algo_size)
        algo = int(data)
        data = struct.pack(format, algo)
        tmpfile.write(data)
        perm = permfile.read(uuid_size)
        tmpfile.write(perm)

        encrypted_wcert = Exec([openssl, algo_dict[algo], '-in', \
                gOptions.wmdrmpd_cert, '-K', key.encode('hex'), '-iv', iv])
        if len(encrypted_wcert) > gCertMaxSize:
            print "Error -> GetEncryptedSBk: encrypted wmdrm \
                    certificate exceeds "+str(gCertMaxSize)+"."
            exit(1)

        tmpfile.write(encrypted_wcert)
        tmpfile.close()
        keyslot = Exec([openssl, 'aes-128-cbc', '-in', \
                'permalgofile.dat', '-K', key.encode('hex'), '-iv', iv])

        length = len(keyslot)
        data = struct.pack(format, length)
        hEncDataFile.write(data)
        hEncDataFile.write(keyslot)

        os.remove('permalgofile.dat')
        tmpfile = open("permalgofile.dat", 'w+')
        data = algofile.read(algo_size)
        algo = int(data)
        data = struct.pack(format, algo)
        tmpfile.write(data)
        perm = permfile.read(uuid_size)
        tmpfile.write(perm)

        encrypted_wkey  = Exec([openssl, algo_dict[algo], '-in', \
                gOptions.wmdrmpd_key, '-K', key.encode('hex'), '-iv', iv])
        if len(encrypted_key) > gKeyMaxSize:
            print "Error -> GetEncryptedSBk: encrypted wmdrm key exceeds " + \
                    str(gKeyMaxSize)+"."
            exit(1)

        tmpfile.write(encrypted_wkey)
        tmpfile.close()
        keyslot = Exec([openssl, 'aes-128-cbc', '-in', \
                'permalgofile.dat', '-K',key.encode('hex'), '-iv', iv])

        length = len(keyslot)
        data = struct.pack(format, length)
        hEncDataFile.write(data)
        hEncDataFile.write(keyslot)

        os.remove('permalgofile.dat')

        tmpfile = open("permalgofile.dat", 'w+')
        data = algofile.read(algo_size)
        algo = int(data)
        data = struct.pack(format, algo)
        tmpfile.write(data)
        perm = permfile.read(uuid_size)
        tmpfile.write(perm)

        encrypted_cert  = Exec([openssl, algo_dict[algo], '-in', \
                gOptions.prdy_cert, '-K', key.encode('hex'), '-iv', iv])
        if len(encrypted_cert) > gCertMaxSize:
            print "Error -> GetEncryptedSBk: encrypted certificate exceeds " \
                    + str(gCertMaxSize)+"."
            exit(1)

        tmpfile.write(encrypted_cert)
        tmpfile.close()
        keyslot = Exec([openssl, 'aes-128-cbc', '-in', \
                'permalgofile.dat', '-K', key.encode('hex'), '-iv', iv])

        length = len(keyslot)
        data = struct.pack(format, length)
        hEncDataFile.write(data)
        hEncDataFile.write(keyslot)

        os.remove('permalgofile.dat')

        tmpfile = open("permalgofile.dat", 'w+')
        data = algofile.read(algo_size)
        algo = int(data)
        data = struct.pack(format, algo)
        tmpfile.write(data)
        perm = permfile.read(uuid_size)
        tmpfile.write(perm)

        encrypted_key   = Exec([openssl, algo_dict[algo], '-in', \
                gOptions.prdy_key, '-K', key.encode('hex'), '-iv', iv])
        if len(encrypted_key) > gKeyMaxSize:
            print "Error -> GetEncryptedSBk: encrypted key exceeds "\
                    + str(gKeyMaxSize)+"."
            exit(1)

        tmpfile.write(encrypted_key)
        tmpfile.close()
        keyslot = Exec([openssl, 'aes-128-cbc', '-in', \
                'permalgofile.dat', '-K', key.encode('hex'), '-iv', iv])

        length = len(keyslot)
        data = struct.pack(format, length)
        hEncDataFile.write(data)
        hEncDataFile.write(keyslot)

        os.remove('permalgofile.dat')

    else:
        algofile.read(4*algo_size)
        permfile.read(4*uuid_size)
        data = struct.pack(format, 0)
        hEncDataFile.write(data)
        hEncDataFile.write(data)
        hEncDataFile.write(data)
        hEncDataFile.write(data)

    if gOptions.widevine_key != None:

        tmpfile = open("permalgofile.dat", 'w+')
        data = algofile.read(algo_size)
        algo = int(data)
        data = struct.pack(format, algo)
        tmpfile.write(data)
        perm = permfile.read(uuid_size)
        tmpfile.write(perm)

        encrypted_widevine_key = Exec([openssl, algo_dict[algo], '-in', \
                gOptions.widevine_key, '-K', key.encode('hex'), '-iv', iv])
        if len(encrypted_widevine_key) > gWidevineMaxSize:
            print "Error -> GetEncryptedSBk: encrypted widevine exceeds "\
                    + str(gWidevineMaxSize)+"."
            exit(1)

        tmpfile.write(encrypted_widevine_key)
        tmpfile.close()
        keyslot = Exec([openssl, 'aes-128-cbc', '-in', \
                'permalgofile.dat', '-K', key.encode('hex'), '-iv', iv])

        length = len(keyslot)
        data = struct.pack(format, length)
        hEncDataFile.write(data)
        hEncDataFile.write(keyslot)

        os.remove('permalgofile.dat')

    else:
        algofile.read(algo_size)
        permfile.read(uuid_size)
        data = struct.pack(format, 0)
        hEncDataFile.write(data)

    if gOptions.nvsi_key != None:

        tmpfile = open("permalgofile.dat", 'w+')
        data = algofile.read(algo_size)
        algo = int(data)
        data = struct.pack(format, algo)
        tmpfile.write(data)
        perm = permfile.read(uuid_size)
        tmpfile.write(perm)

        encrypted_nvsi_key = Exec([openssl, algo_dict[algo], '-in', \
                gOptions.nvsi_key, '-K', key.encode('hex'), '-iv', iv])
        if len(encrypted_nvsi_key) > gNVSIMaxSize:
            print "Error -> GetEncryptedSBk: encrypted nvsi exceeds "\
                    + str(gNVSIMaxSize)+"."
            exit(1)

        tmpfile.write(encrypted_nvsi_key)
        tmpfile.close()
        keyslot = Exec([openssl, 'aes-128-cbc', '-in', \
                'permalgofile.dat', '-K', key.encode('hex'), '-iv', iv])

        length = len(keyslot)
        data = struct.pack(format, length)
        hEncDataFile.write(data)
        hEncDataFile.write(keyslot)

        os.remove('permalgofile.dat')

    else:
        algofile.read(algo_size)
        permfile.read(uuid_size)
        data = struct.pack(format, 0)
        hEncDataFile.write(data)

    if gOptions.hdcp2_sink_key != None:

        tmpfile = open("permalgofile.dat", 'w+')
        data = algofile.read(algo_size)
        algo = int(data)
        data = struct.pack(format, algo)
        tmpfile.write(data)
        perm = permfile.read(uuid_size)
        tmpfile.write(perm)

        encrypted_hdcp2_sink_key = Exec([openssl, algo_dict[algo], '-in', \
                gOptions.hdcp2_sink_key, '-K', key.encode('hex'), '-iv', iv])
        if len(encrypted_hdcp2_sink_key) > gHDCP2MaxSize:
            print ("Error -> GetEncryptedSBk: encrypted hdcp2 exceeds "
                            + str(gHDCP2MaxSize) + ".")
            exit(1)

        tmpfile.write(encrypted_hdcp2_sink_key)
        tmpfile.close()
        keyslot = Exec([openssl, 'aes-128-cbc', '-in', \
                'permalgofile.dat', '-K', key.encode('hex'), '-iv', iv])

        length = len(keyslot)
        data = struct.pack(format, length)
        hEncDataFile.write(data)
        hEncDataFile.write(keyslot)

        os.remove('permalgofile.dat')

    else:
        algofile.read(algo_size)
        permfile.read(uuid_size)
        data = struct.pack(format, 0)
        hEncDataFile.write(data)

    # End with slots
    hEncDataFile.seek(0)
    data = hEncDataFile.read()

    crc = zlib.crc32(data)

    format = "i"
    crcData = struct.pack(format, crc)
    hEncDataFile.write(crcData)
    hEncDataFile.close()

    eks_size = os.stat("eks_temp.dat").st_size

    hEncDataFile = open ("eks_temp.dat",'rb')
    data = hEncDataFile.read()
    hEncDataFile.close()

    hEksFile = open ("eks.dat",'wb')
    sizeData = struct.pack(format, eks_size)
    hEksFile.write(sizeData)
    hEksFile.write(data)
    hEksFile.close()

    os.remove("eks_temp.dat")
    os.remove("temp_perm_file.dat")

#------------------------------------------
# Main function, script entry point
#------------------------------------------
def main():
    print ""
    ParseCmd()
    GetEncryptedSBK()

    print "Encrypted data file created."
main()
