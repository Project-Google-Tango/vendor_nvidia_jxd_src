
Tegrastats
----------------------------------------------------------
Tegrastats is a program that runs on Android and reports memory and
dynamic frequency scaling (DFS) statistics for NVIDIA's Tegra based devices.
When you run it, it will start printing statistics into Android log (logcat).

Example log print:
E/TegraStats( 3627): RAM 250/490MB (lfb 17x1MB) Carveout 12/12MB (lfb 0kB) GART 28/32MB (lfb 4MB) IRAM 36/256kB (lfb 128kb) | DFS(%@MHz): cpu [61%,45%]@835 avp 17%@100 vde 34%@216 emc 25%@333

The statistics are:

  RAM X/Y (lfb NxZ)
    X is the amount of RAM in use in megabytes.
    Y is the total amount of RAM available for applications.
    Z is the size of the largest free block, N the number of free blocks of
       this size. Lfb is a statistic about the memory allocator, and refers to
       the largest contiguous block of physical memory that can currently be
       allocated (note the word "physical", allocations in virtual memory can
       be bigger). It is at most 4MB, and can become smaller with memory
       fragmentation.

  Carveout X/Y (lfb Z)
    ("carveout" refers to video memory, or GPU memory)
    X is the amount of carveout memory in use, in megabytes.
    Y is the total amount available.
    Z is the size of the largest free block. Lfb is a statistic about the memory
       allocator, and refers to the largest contiguous memory block that can currently
       be allocated. It is usually equal or slightly smaller than the amount of free
       memory, but can be smaller in case the memory is fragmented.

  GART X/Y (lfb Z)
    ("GART" refers to system memory that has been mapped to be visible to
     the GPU.  It is typically allocated when carveout allocations fail.)
    X is the amount of GART memory in use, in megabytes.
    Y is the total amount available.
    Z is the size of the largest free block. Lfb is a statistic about the memory
       allocator, and refers to the largest contiguous memory block that can currently
       be allocated. It is usually equal or slightly smaller than the amount of free
       memory, but can be smaller in case the memory is fragmented.

  IRAM X/Y (lfb Z)
    ("IRAM" refers to memory local to the video hardware engine)
    X is the amount of IRAM memory in use, in kilobytes.
    Y is the total amount available.
    Z is the size of the largest free block. Lfb is a statistic about the memory
       allocator, and refers to the largest contiguous memory block that can currently
       be allocated. It is usually equal or slightly smaller than the amount of free
       memory, but can be smaller in case the memory is fragmented.

  cpu [X%,Y%]@Z
    X&Y are the load statistics for each of the CPU cores relative to the current
      running frequency Z, or 'off' in case a core is currently powered down. This
      is a rough approximation based on time spent in system idle process as reported
      by the Linux kernel in /proc/stat file.
    Z is the CPU frequency in MHz (the frequency will dynamically go up or down
      depending on what workload the CPU is seeing).

  avp X%@Y
    ("AVP" is the audio/video processor, which is not visible to either
     the OS or applications.  However, some forms of video decode/encode
     make heavy use of it.)
    X is the percent of the AVP that is being used (aka CPU load), relative to the
      current running frequency Y.
    Y is the AVP frequency in MHz (the frequency will dynamically go up or down
      depending on what workload the AVP is seeing).

  vde X%@Y
    ("VDE" is the video hardware engine.)
    X is the percent of the VDE that is being used (aka CPU load), relative to the
      current running frequency Y.
    Y is the VDE frequency in MHz.

  emc X%@Y
    ("EMC" is the external memory controller, which all sysmem/carveout/GART
     memory accesses go through)
    X is the percent of the EMC memory bandwidth that is being used, relative to the
      current running frequency Y.
    Y is the EMC frequency in MHz.


NOTE: If DFS is disabled, AVP, VDE, and EMC workload statistics are meaningless.


Installing Tegrastats
-----------------------------
adb remount
adb push tegrastats /system/bin


Starting Tegrastats
-----------------------------
adb shell tegrastats <delay> &
adb logcat

where <delay> is the frequency of log prints, expressed in milliseconds (1000 means print every second).

Also the tegrastats can be started in background, with the option of output to 
logfile as below:

(1.) run tegrastats in background, output would still be visiable in logcat:
adb shell tegrastats --start 
adb logcat

(2.) run tegrastats in background and dump output into logfile.txt 
adb shell tegrastats --start --logfile /path/to/logfile.txt

Stopping Tegrastats
-----------------------------
adb shell ps
adb shell kill <pid>

where <pid> is the process id of tegrastats as reported by the ps command.

Alterlatively it can be stopped with:
adb shell tegrastats --stop


Using Tegrastats to max out the clock frequencies
-----------------------------

The command
adb shell tegrastats -max

will force all DFS controlled clock frequencies to their maximum values, effectively disabling DFS.
When run this way, tegrastats will quit immediately after setting the clocks.
Note: this option is provided only for testing purposes. Disabling DFS will cause the device battery to run out quickly.

To return DFS back to normal, run
adb shell tegrastats -default

