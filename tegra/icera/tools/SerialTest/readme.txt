This application schedules periodic wakes from any power mode.
The period is 30s by default.

The period can be set through the following command line (example to set period to 25000ms):

adb shell "am start -a android.intent.action.MAIN -n modem.SerialTest/modem.SerialTest.StartApplication -e period 25000"


