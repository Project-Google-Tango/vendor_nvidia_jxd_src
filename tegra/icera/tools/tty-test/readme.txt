TTY Test Application

This application can be used to test the TTY interface to the Icera modem.

Usage:
   /system/bin/ttytestapp -d /dev/ttyS0
      -d /dev/name    = Device name to use (required)
      -r xx           = repeat test count (optional, default 100)
      -c              = continuous test (optional, default off)
      -l xx           = min size of large transfers (optional, default 256)
      -s xx           = max size of large transfers (optional, default 256)
      -m xx           = min pause time (ms) between requests (optional, default 0)
      -n xx           = max pause time (ms) between requests (optional, default 0)
      -t xx           = max receive timeout for target response (ms) (optional, default 2000ms)
      -a              = Skip AT%LOOPBACK request (optional, default AT%LOOPBACK request sent)
      -q              = Quiet mode (reduced logging) (optional, default off)
      -i x            = Test hibernation in given mode (0, 1 or 4)
                        Incompatible with loopback mode...
      -j x            = Set max sleep time in ms.
      -u x            = Target uploads data every x ms using AT%IDUMMYU=<x>,1,1 from host
      -z xx           = Test number to run
         1            = Sequential mode - send request - wait for response before moving on (optional, default)
         2            = Parallel mode (small xfer) - send requests without waiting (test large quantity of small xfers)
         3            = Parallel mode (variable xfer) - send requests without waiting (test flow control / max throughput)
         4            = Hibernate test
         5            = Reset/Reboot test
      -w xx           = Maximum number of pending writes (optional, default 10)

Long options:
      --no_smt        = No mode switch in reset/reboot test
      --no_ct         = No crash test in reset/reboot test
      --switch_wait x = Time to wait after a mode switch before any action
                        In ms, default is 15000ms
      --crash_wait x  = Time to take after a crash to try any action on device
                        In ms, default is 120000ms.
                        After this time, device is considered as out of order.

How to test HLP?
~~~~~~~~~~~~~~~

After stopping RIL by doing:
adb root && ping -n 3 127.0.0.1 >nul && adb shell stop ril-daemon

Send the following command:
adb shell "/system/bin/ttytestapp -d /dev/ttyHLP1 -r 100000 -z 3 -l 1 -s 1024 -n 20"

Verify that the test completes without error.


How to test hibernate?
~~~~~~~~~~~~~~~~~~~~~

After stopping RIL by doing:
adb root && ping -n 3 127.0.0.1 >nul && adb shell stop ril-daemon

Send the following command:
adb shell "/system/bin/ttytestapp -d /dev/ttyHLP1 -r 1000 -m 4000 -n 6000 -z 4 -i 1 -u 3000"

Verify that the test completes without error. Optionally, verify the NWSTATE notifications and check that the modem
does not drop off the network intermittently.


How to test Livanto platform reboot/reset ?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After stopping RIL by doing:
adb root && ping -n 3 127.0.0.1 >nul && adb shell stop ril-daemon

Send the following command:
adb shell "/system/bin/ttytestapp -d /dev/ttyHLP1 -r 1000 -z 5"

Verify that the test completes without error.
This test stress both mode switching and modem recovery after any assert/afault.

To test only recovery after mode switch, send following command:
adb shell "/system/bin/ttytestapp -d /dev/ttyHLP1 -r 1000 --no_ct -z 5"

To test only recovery after assert/afault, without the need to stop RIL, send following command:
adb shell "/system/bin/ttytestapp -d /dev/ttyHLP3 -r 1000 --no_smt -z 5"





