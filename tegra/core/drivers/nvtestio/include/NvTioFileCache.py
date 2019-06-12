# 
# Copyright (c) 2008 NVIDIA Corporation.  All Rights Reserved.
# 
# NVIDIA Corporation and its licensors retain all intellectual property and
# proprietary rights in and to this software and related documentation.  Any
# use, reproduction, disclosure or distribution of this software and related
# documentation without an express license agreement from NVIDIA Corporation
# is strictly prohibited.
# 

import struct
import pdb
import os

# This magic blob is a fully formed ARM ELF file defining one constant
# static character array s_TioFileCache, and one extern'ed array of
# NvTioFileCache structs called g_NvTioFileCache.
#
# We're going to emit this ELF file, emit new values for the two arrays,
# and then seek backwards to patch the ELF file to point to our new arrays.
# We'll also need to rewrite a section of relocation data  because the
# contents of g_NvTioFileCache point into s_TioFileCache
_o_header = ''.join(chr(c) for c in (127, 69, 76, 70, 1, 1, 1, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 1, 0, 40, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0,
    0, 0, 0, 4, 52, 0, 32, 0, 0, 0, 40, 0, 8, 0, 7, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 2, 5, 0, 0, 4, 0, 0, 0, 2, 5, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 0, 241, 255, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    241, 255, 125, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 3, 0, 1, 0, 136, 0, 0, 0, 0,
    0, 0, 0, 8, 0, 0, 0, 3, 0, 2, 0, 142, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 1, 0, 154, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 17, 2, 2, 0, 0, 95, 116, 
    105, 111, 95, 102, 99, 97, 99, 104, 101, 46, 115, 0, 66, 117, 105, 108, 100,
    65, 116, 116, 114, 105, 98, 117, 116, 101, 115, 36, 36, 65, 82, 77, 95, 
    73, 83, 65, 118, 52, 36, 77, 36, 80, 69, 36, 65, 58, 76, 50, 50, 36, 88, 
    58, 76, 49, 49, 36, 83, 50, 50, 36, 73, 69, 69, 69, 49, 36, 126, 73, 87, 
    36, 85, 83, 69, 83, 86, 54, 36, 126, 83, 84, 75, 67, 75, 68, 36, 85, 83, 
    69, 83, 86, 55, 36, 126, 83, 72, 76, 36, 79, 83, 80, 65, 67, 69, 36, 69, 
    66, 65, 56, 36, 80, 82, 69, 83, 56, 36, 69, 65, 66, 73, 118, 50, 0, 46, 99, 
    111, 110, 115, 116, 100, 97, 116, 97, 0, 46, 100, 97, 116, 97, 0, 115, 95, 
    70, 105, 108, 101, 67, 97, 99, 104, 101, 0, 103, 95, 78, 118, 84, 105, 111,
    70, 105, 108, 101, 67, 97, 99, 104, 101, 0, 0, 65, 82, 77, 47, 84, 104, 117,
    109, 98, 32, 77, 97, 99, 114, 111, 32, 65, 115, 115, 101, 109, 98, 108, 101,
    114, 44, 32, 82, 86, 67, 84, 50, 46, 50, 32, 91, 66, 117, 105, 108, 100, 32,
    51, 52, 57, 93, 0, 45, 111, 95, 116, 105, 111, 95, 102, 99, 97, 99, 104, 101,
    46, 111, 32, 95, 116, 105, 111, 95, 102, 99, 97, 99, 104, 101, 46, 115, 0, 0,
    0, 0, 0, 46, 99, 111, 110, 115, 116, 100, 97, 116, 97, 0, 46, 100, 97, 116, 
    97, 0, 46, 114, 101, 108, 46, 100, 97, 116, 97, 0, 46, 115, 121, 109, 116, 97,
    98, 0, 46, 115, 116, 114, 116, 97, 98, 0, 46, 99, 111, 109, 109, 101, 110,
    116, 0, 46, 115, 104, 115, 116, 114, 116, 97, 98, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 52, 0, 0,
    0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 12, 0, 0, 0, 1,
    0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 60, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 4, 0, 0, 0, 0, 0, 0, 0, 18, 0, 0, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 68,
    0, 0, 0, 16, 0, 0, 0, 4, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 28, 0, 0,
    0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 84, 0, 0, 0, 112, 0, 0, 0, 5, 0, 0, 0, 6,
    0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 36, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 196, 0, 0, 0, 171, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    44, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 112, 1, 0, 0, 80, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 53, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 192, 1, 0, 0, 63, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0))

# offsets into the ELF file at which we'll need to patch stuff
_o_header_offsets = {
 's_TioFileCache':512 + 1*40 + 16,
 'g_NvTioFileCache':512 + 2*40 + 16,
 'o_reldata':512 + 3*40 + 16
}

def emit(fnames, outf):
    file_records = []

    # write out our canned ELF file
    outf.write(_o_header)

    # append s_TioFileCache, an constant character array
    # containing the name and contents of the files
    # we're baking.
    s_TioFileCache_offset = outf.tell()
    s_TioFileCache_len = 0
    for num, name in enumerate(fnames):
        name = name.split(':')
        name.append(name[0])
        cached_name, real_name = name[0:2]

        if not os.path.exists(real_name):
            print "file doesn't exist: %s" % real_name
            continue

        cached_name += '\0'
        outf.write(cached_name)
        
        f = file(real_name,'rb')
        flen = 0
        while True:
            dat = f.read(1024)
            flen += len(dat);
            if len(dat)==0:
                break;
            outf.write(dat)

        #record the file name and offset
        file_records.append(3*[0])
        file_records[-1][0] = s_TioFileCache_len
        s_TioFileCache_len += len(cached_name)
        file_records[-1][1] = s_TioFileCache_len
        file_records[-1][2] = flen 
        s_TioFileCache_len += flen
        print "file open: %s" % real_name

    #pad s_TioFileCache up to the next 32-bit boundary
    padding = '\0\0\0\0'[s_TioFileCache_len & 3:]
    outf.write(padding)
    s_TioFileCache_len += len(padding)

    # append g_NvTioFileCache, an array of NvTioFileCache structs
    # which point into s_NvTioFileCache.  We don't know the address
    # of s_NvTioFileCache so we'll need to supply the ELF file with
    # relocation information
    g_NvTioFileCache_offset = outf.tell()
    g_NvTioFileCache_len = 0
    for r in file_records:
        outf.write(struct.pack('<IIII', r[0], r[1], r[2], 0))
        g_NvTioFileCache_len += 16

    outf.write(struct.pack('<IIII', 0, 0, 0, 0))
    g_NvTioFileCache_len += 16

    # append the relocation information
    o_reldata_offset = outf.tell()
    o_reldata_len = 0
    for i in range(len(file_records)):
        outf.write(struct.pack('<IIII', 16*i, 0x502, 16*i+4, 0x502))
        o_reldata_len += 16

    # go back and patch the offsets and sizes of the
    # three ELF sections we've just written
    outf.seek(_o_header_offsets['s_TioFileCache'])
    outf.write(struct.pack('<I',s_TioFileCache_offset))
    outf.write(struct.pack('<I',s_TioFileCache_len))

    outf.seek(_o_header_offsets['g_NvTioFileCache'])
    outf.write(struct.pack('<I',g_NvTioFileCache_offset))
    outf.write(struct.pack('<I',g_NvTioFileCache_len))

    outf.seek(_o_header_offsets['o_reldata'])
    outf.write(struct.pack('<I',o_reldata_offset))
    outf.write(struct.pack('<I',o_reldata_len))

if __name__ == '__main__':
    import sys
    import os

    #pdb.set_trace()
    args = sys.argv[2:]
    if args[0] == '-filelist':
        filename = args[1]
        # read all files in <filename>
        f = open(filename, 'r')
        data = f.read()
        files = data.split('\n')
        f.close()
    else:
        files = args[1:]

    args = list(set(files))
    args.sort()
#    pdb.set_trace()
    
    outf = file(sys.argv[1],'wb')
    try:
        emit(args, outf)
    except:
        outf.close()
        os.unlink(sys.argv[1])
        raise
