#!/system/bin/sh
# From netd capture on jb-mr2
#
# Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
#

TESTMODE=`getprop persist.modem.icera.testmode`

if [ $TESTMODE -eq 1 ]; then
  # TESTMODE=1
  if [ $# -ne 0 ] && [ $# -ne 2 ] ; then
    echo "Usage: nwifdown [ <network interface> <gateway> ]"
    exit 1
  fi

  NWIF=$1
  if [ -z "$NWIF" ]; then
    NWIF="wwan0"
  fi
  GW=$2

  echo "network interface : $NWIF"
  echo "gateway           : $GW"
  echo ""
fi

if [ $TESTMODE -eq 2 ]; then
  # TESTMODE=2
  if [ $# -ne 0 ] && [ $# -ne 1 ] ; then
    echo "Usage: nwifdown [ <network interface> ]"
    exit 1
  fi

  NWIF=$1
  if [ -z "$NWIF" ]; then
    NWIF="wwan0"
  fi

  echo "network interface : $NWIF"
  echo ""
fi

echo "Untether interface $NWIF"
ndc interface list
ndc nat disable rndis0 $NWIF 0
ndc tether interface remove rndis0
ndc interface list
ndc tether stop
ndc ipfwd disable

if [ $TESTMODE -eq 1 ]; then
  echo "Remove routes"
  if [ -z "$GW" ]; then
    ndc interface route remove $NWIF default 0.0.0.0 0 $GW
  fi
  ndc resolver flushif $NWIF
  ndc bandwidth removeiquota $NWIF
  ndc bandwidth setglobalalert 2097152

  echo "Bring down interfaces"
  ndc interface setcfg rndis0 down
  ndc interface setcfg $NWIF down
  ndc interface setcfg rndis0 down
fi

