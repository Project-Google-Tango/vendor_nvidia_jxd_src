#!/system/bin/sh
# From netd capture on jb-mr2
#
# Copyright (c) 2013-2014, NVIDIA CORPORATION.  All rights reserved.
#

# TESTMODE = 1, RIL/framework disabled - manual network interface and route config
# TESTMODE = 2, RIL/framework in charge of network interface and route config

TESTMODE=`getprop persist.modem.icera.testmode`

if [ $TESTMODE -eq 1 ]; then
  # TESTMODE=1
  if [ $# -lt 5 ]; then
    echo "Usage: nwifup <IP> <gateway> <subnet prefix> <DNS1> <DNS2> [ <tether 0:1> [ <network interface> ] ]"
    exit 1
  fi

  IP=$1
  GW=$2
  PREFIX=$3
  DNS1=$4
  DNS2=$5
  TETHER=$6
  if [ -z "$TETHER" ]; then
    TETHER=0
  fi
  NWIF=$7
  if [ -z "$NWIF" ]; then
    NWIF="wwan0"
  fi

  echo "IP                : $IP"
  echo "gateway           : $GW"
  echo "subnet prefix     : $PREFIX"
  echo "DNS1              : $DNS1"
  echo "DNS2              : $DNS2"
  echo "tether            : $TETHER"
  echo "network interface : $NWIF"

  echo ""

  echo "Bring up interface $NWIF"
  ndc interface setcfg $NWIF $IP $PREFIX up
  echo "Routes configuration"
  ndc interface list
  ndc bandwidth enable
  ndc firewall disable
  ndc bandwidth setglobalalert 2097152
  ndc firewall disable
  ndc resolver setifdns $NWIF $DNS1 $DNS2
  ndc resolver setdefaultif $NWIF
  ndc interface route add $NWIF default $GW 32 0.0.0.0
  ndc interface route add $NWIF default 0.0.0.0 0 $GW
  ndc resolver setifdns $NWIF $DNS1 $DNS2
  # ndc resolver setifaceforpid $NWIF 685
  ndc interface route add $NWIF default $GW 32 0.0.0.0
  ndc interface route add $NWIF default $DNS1 32 $GW
  ndc interface route add $NWIF default $GW 32 0.0.0.0
  ndc interface route add $NWIF default $DNS2 32 $GW
  ndc interface route add $NWIF secondary $GW 32 0.0.0.0
  ndc interface route add $NWIF secondary 0.0.0.0 0 $GW
  ndc bandwidth setiquota $NWIF 9223372036854775807
  ndc bandwidth removeiquota $NWIF
  ndc bandwidth setiquota $NWIF 9223372036854775807
  ndc bandwidth removeiquota $NWIF
  ndc bandwidth setiquota $NWIF 9223372036854775807
  ndc bandwidth removeiquota $NWIF
  ndc bandwidth setiquota $NWIF 9223372036854775807
  ndc interface route add $NWIF default $GW 32 0.0.0.0
  ndc bandwidth setglobalalert 2097152
  #ndc interface route add $NWIF default 173.194.34.34 32 $GW
  ndc bandwidth setglobalalert 2097152
  # ndc resolver clearifaceforpid 685
  ndc interface route remove $NWIF secondary $GW 32 0.0.0.0
  ndc interface route remove $NWIF secondary 0.0.0.0 0 $GW
  ndc interface route remove $NWIF default $DNS1 32 $GW
  ndc interface route remove $NWIF default $DNS2 32 $GW
  ndc resolver flushif $NWIF
  ndc bandwidth removeiquota $NWIF
  ndc bandwidth setiquota $NWIF 9223372036854775807
  ndc bandwidth removeiquota $NWIF
  ndc bandwidth setiquota $NWIF 9223372036854775807
  ndc bandwidth setglobalalert 2097152
  ndc bandwidth setglobalalert 2097152

  if [ $TETHER -ne 1 ]; then
    echo "No tethering - exit"
    exit 1
  fi
else
  # TESTMODE=2
  if [ $# -ne 0 ] && [ $# -ne 1 ] ; then
    echo "Usage: nwifup [ <network interface> ]"
    exit 1
  fi

  NWIF=$1
  if [ -z "$NWIF" ]; then
    NWIF="wwan0"
  fi

  echo "network interface : $NWIF"
  echo ""
fi

echo "Tether interface $NWIF"
ndc interface list
ndc interface list
ndc interface getcfg rndis0
ndc interface setcfg rndis0 192.168.42.129 24 multicast up broadcast
ndc tether interface add rndis0
ndc ipfwd enable
ndc tether start 192.168.42.2 192.168.42.254 192.168.43.2 192.168.43.254 192.168.44.2 192.168.44.254 192.168.45.2 192.168.45.254 192.168.46.2 192.168.46.254 192.168.47.2 192.168.47.254 192.168.48.2 192.168.48.254
ndc tether dns set 8.8.8.8 8.8.4.4
if [ -n "$DNS1" -a -n "$DNS2" ]; then
  ndc tether dns set $DNS1 $DNS2
fi
ndc nat enable rndis0 $NWIF 2 fe80::/64 192.168.42.0/24


