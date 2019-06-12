#!/system/bin/sh
#
# Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
#

# TESTMODE = 1, RIL stopped, reuse its tty device
# TESTMODE = 2, RIL running concurrently, use /dev/at_modem

TESTMODE=`getprop persist.modem.icera.testmode`

if [ $TESTMODE -eq 1 ]; then
  AT_PORT=`getprop ro.ril.devicename`
else
  AT_PORT="/dev/at_modem"
fi

/system/bin/ttyfwd -d /dev/ttyGS0 -d $AT_PORT
