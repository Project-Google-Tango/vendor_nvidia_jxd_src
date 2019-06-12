# test-mode/README.TXT
#
# Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
#

nwifup and nwifdown are helper scripts to enable and disable the RNDIS link
when platform is configured in modem test mode.

nwifup should be used when PDP context has been activated. It takes care of
necessary actions to enable tethering over USB RNDIS:
- modem network interface config and bring up
- rndis network interface config and bring up
- tethering activation (routing table, IP forward, DHCP...)

nwifdown can be used to deactivate the link and tear down modem and rndis
interfaces.

Usage:
# nwifup.sh <IP> <gateway> <subnet prefix> <DNS1> <DNS2> [ <tether 0:1> [ <network interface> ] ]
# nwifdown.sh [ <network interface> <gateway> ]


Example:

[ PDP context activation - retrieve IP parameters ]

AT+CGDCONT=2,"IP","orange"
OK
AT%IPDPCFG=1,0,0
OK
AT%IPDPACT=2,1,"IP",0
%IPDPACT: 2, 2, 0
OK
%IPDPACT: 2, 1, 0
AT%IPDPADDR?
%IPDPADDR: 2, 10.75.156.242, 10.75.156.243, 192.168.10.110, 0.0.0.0, 0.0.0.0, 0.0.0.0, 255.255.255.254, 10.75.156.243
OK


[ Issue command on shell prompt to start tethering over RNDIS ]
[ Note: parameter '31' is subnet prefix for subnet mask 255.255.255.254 ]

# nwifup.sh 10.75.156.242 10.75.156.243 31 192.168.10.110 0.0.0.0 1


[ To stop tethering and disable network interfaces ]

# nwifdown.sh

