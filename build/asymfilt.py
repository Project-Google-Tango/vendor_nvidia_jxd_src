#!/usr/bin/env python
#
# Copyright (c) 2011 NVIDIA Corporation.  All Rights Reserved.
#
# NVIDIA Corporation and its licensors retain all intellectual property
# and proprietary rights in and to this software, related documentation
# and any modifications thereto.  Any use, reproduction, disclosure or
# distribution of this software and related documentation without an express
# license agreement from NVIDIA Corporation is strictly prohibited.
#

import os
import re
import subprocess
import sys
import bisect

import logging
# Uncomment to enable logging to stderr.
#logging.basicConfig(level=logging.DEBUG)

android_product_out = os.environ['ANDROID_PRODUCT_OUT']
symbols_dir = os.path.join(android_product_out, 'symbols')

class Mode:
    none = 0
    default = 1
    tombstone = 2

def stdout_of_cmd(*args):
    return subprocess.Popen(args,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE).communicate()[0]

#def adb(*args):
#    return stdout_of_cmd('adb', *args)

class IntervalMap:
    def __init__(self):
        self.ranges = []
    def __setitem__(self, _slice, v):
        self.ranges.append((_slice.start, _slice.stop, v))
    def finalize(self):
        self.starts, self.stops, self.values = zip(*sorted(self.ranges))
    def __getitem__(self, k):
        pos = bisect.bisect_right(self.starts, k)
        if pos == 0:
            return None
        pos = pos - 1
        if k >= self.stops[pos]:
            return None
        return self.values[pos]

def add_map(im, line):
    try:
        if line[20] == 'x' and line[49] == '/':
            start = int(line[0:8], 16)
            end = int(line[9:17], 16)
            module = line[49:]
            # hack hack hack, I don't know why I need this...
            # executables don't get loaded the same way as shared objects.
            # also, I'm not taking into account the offset field,
            # that's bound to screw something up
            im[start:end] = (module, start if line.endswith('.so') else 0)
    except:
       pass

cppRE = re.compile(r'(.*) \([+\w]*\)')
def addr2line(symbols_dir, module, offset):
    try:
        m = cppRE.match(module)
        if m:
          module = m.group(1)
        module_dir, module_name = os.path.split(module)
        symfile = symbols_dir + module
        if not os.path.isfile(symfile):
            # symbol files get splatted into a flat directory so look in parent dir too
            module_dir = os.path.dirname(module_dir)
            symfile = symbols_dir + os.path.join(module_dir, module_name)
            assert os.path.isfile(symfile)

        # -C for demangling, -f for functions
        stuff = stdout_of_cmd('arm-linux-androideabi-addr2line', '-C', '-f', '-e',
                              symfile, hex(offset))

        func, line = stuff.splitlines()
        try:
            line = line[line.index('/mobile/')+8:]
        except:
            pass

        return module_name + ':' + func + '():' + line

    except:
        return '%s + %s (no symbols found)' % (module, hex(offset))


def android_lookup(line, header, info, rexp):
    match = rexp.match(info)
    if not match:
        return line

    beginning, addr, module = match.groups()
    addr = int(addr, 16)
    # want the branch, not the return address
    if addr:
        addr -= 1
    ## Dodgy, but it makes executables sometimes look up :(
    ## But it makes others get worse...  Weird
    #    if not module.endswith('.so'):
    #        addr += 0x8000

    logging.debug( 'Looking up ' + module + ' in addr ' + str(addr))
    lookup = addr2line(symbols_dir, module, addr)

    return header + beginning + lookup


def main():

    ####################################################
    # regexp that detects ADB mode
    # I/DEBUG   (  774): pid: 1576, tid: 1590  >>> com.android.browser <<<
    # <--ADB HEADER-G1-><----------------- INFO G3----------------------->
    # Pid will be in group 2
    rexp_adb = re.compile('(./.*\(\s*(\d+)\):)\s?(.*)')

    ####################################################
    # regexps with _start and _end define where interesting
    # stuff begins and ends. Applied to INFO part

    ## Application mode
    #  pid: 1576, tid: 1590  >>> com.android.browser <<<
    rexp_tomb_start = re.compile('pid.*tid.*')
    # code around lr:
    rexp_tomb_end   = re.compile('code around lr:')
    #           #01  pc 0014a784  /system/lib/libwebcore.so
    # GROUPS:   -- 1 --><- 2 -->  <---------- 3 ---------->
    rexp_tomb = re.compile('(\s+ #\d+ \s* \w\w )([\w]+)\s+(/.*)$')

    # Android callstack mode
    rexp_android = re.compile('([^#]*#\d+ \s* \w\w )([\w]+)\s+(/.*)$')

    ## Default mode
    rexp_default_start = re.compile('Callstack:')
    # rexp_default = implemented in different manner
    rexp_default_end = re.compile('^$')

    if not os.path.exists(symbols_dir):
        print '$ANDROID_PRODUCT_OUT/symbols does not exist ('+ symbols_dir + ')'
        return

    pstate = {}
    mode = Mode.none

    while 1:

        line = sys.stdin.readline()
        if not line:
            break
        line = line.rstrip()

        # Detect whether we have input from ADB or elsewhere
        adbmatch = rexp_adb.match(line)

        if adbmatch:
            header, pid, info = adbmatch.groups()
        else:
            header, pid, info = '', None, line

        ### Simple state machine

        if header.startswith('D/CallStack('):
            line = android_lookup(line, header, info, rexp_android)

        ########################
        elif mode == Mode.none:

            logging.debug(info)

            # Try searching for keywords that start modes
            if rexp_tomb_start.match(info):
                mode = Mode.tombstone

            elif rexp_default_start.match(info):
                mode = Mode.default
                im = None

        ############################
        elif mode == Mode.tombstone:
            logging.debug( 'Tombstone mode ' + info )

            # Check if we should stop
            if rexp_tomb_end.match(info):
                mode = Mode.none

            else:
                line = android_lookup(line, header, info, rexp_tomb)

        ##########################
        elif mode == Mode.default:

            # If we reach the empty line after stack dump.
            if rexp_default_end.match(info):
                mode = Mode.none
                continue

            try:
                try:
                    im, pm = pstate[pid]
                except:
                    im, pm = None, False

                if info.startswith('BeginProcMap'):
                    im = IntervalMap()
                    pm = True
                    line = None
                elif info.startswith('EndProcMap'):
                    im.finalize()
                    pm = False
                    line = None
                elif pm:
                    add_map(im, info)
                    line = None
                elif im is not None:
                    try:
                        pos = info.index('0x')+2
                        addr = int(info[pos:], 16)
                        module, start = im[addr]
                        if addr > start:
                            addr = addr - 1
                        lookup = addr2line(symbols_dir, module, addr - start)
                        line = header + ' ' + lookup
                    except:
                        im = None

                pstate[pid] = (im, pm)
            except:
                pass


        ## Print the line.
        if line is not None:
            print line
            sys.stdout.flush()

if __name__=='__main__':
   main()
