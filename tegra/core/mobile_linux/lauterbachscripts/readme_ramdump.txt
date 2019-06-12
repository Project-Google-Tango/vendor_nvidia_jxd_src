---------------------------------------------------------------
 Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
---------------------------------------------------------------

Lauterbach RAMdump scripts guide V0.3

0.	History
Dec-07-2012 :	V0.3	: T114 verified
Jan-30-2012 :	V0.2	: scripts are adjusted (dumping core registers)
Jan-17-2012 :	V0.1	: batch files for windows Trace32 are added
Jan-13-2012 :	Draft	: Initial Release

1.	Preface
Sometimes, we need RAMdump for further investigating target crash/hang issue or
parallel debugging them with many co-workers.
This document explains how to use RAMdump scripts with Lauterbach Trace32.
It just uses Trace32 only without any target SW changes and this kind of
approach has been used in the mobile industries for a long time.
However, As RAM size of mobile products become larger, many OEM customers
prefer faster dump method like as dump via USB interface because direct
ramdump using Trace32 takes long time.
JTAG interface is slower than USB interface or accessing storage memory thing.

2.  File lists
	config_ramdump.t32       : config Trace32 for ramdump
	cpu_menu_setup.cmm       : Trace32 menu setup
	install_customer_scripts : install shell scripts (linux only)
	install_scripts          : install shell scripts (linux only)
	load_ramdump.cmm         : restore RAMdump data
	ramdump_coresel.cmm      : select CPU core
	ramdump_menu_setup.cmm   : Trace32 menu setup
	ramdump_var.cmm          : set global variables for RAMdump
	ramdump_winset.cmm       : Trace32 windows setting example for RAMdump
	save_ramdump.cmm         : save RAMdump data
	t32.cmm                  : main Trace32 cmm script
	t32ramdump 	             : RAMdump debugging shell script (linux only)
	user_config.cmm          : user config script
	config_cpu_win.t32       : config Trace32 (windows only)
	ramdump_setpath.cmm      : rematch directory of symbol (windows only)
	envtegra.bat             : setup environment (windows only)
	t32cpu.bat               : Trace32 debugging batch file (windows only)
	t32ramdump.bat           : RAMdump debugging master batch file (windows only)

3.	Prerequisite for RAMdump scripts
3.1.	Install Lauterbach Trace32 Software (Linux machine)
About installation for Trace32 SW, you can consult your local Trace32 distibuter.
If you already installed Trace32, please skip this chapter and jump to 3.2.

3.1.1.	Installation T32 on PC linux
If you are using shared PC or Server, t32 may be installed at /opt/t32 or /home/t32 or /usr/local/t32 etc.
In this case, you should set write permissions for your group or others. (i.e. chmod -R o+w *).
If you have your own PC, I recommend you to install t32 in your home folder (i.e. /home/[username]/t32).
If you set environment variable correctly, $T32SYS should be /home/[username]/t32.

3.1.2.	Install T32 scripts for Tegra android BSP
Setup build environment your Tegra Android BSP.

cd $TEGRA_ROOT/mobile_linux/lauterbachscripts
; $TEGRA_ROOT is android/vendor/nvidia/tegra/core

./install_scripts

Edit $T32SYS/user_config.cmm to match your particular build and debug environment

3.1.3.	Verifying T32 SW running
cd $T32SYS
t32marm & or t32cpu &

3.2.	Install RAMdump scripts
3.2.1.	Linux Machine
Now If you are in android top folder, please execute following install scripts.
./vendor/nvidia/tegra/core/mobile_linux/lauterbachscripts/install_scripts
With this, needed scripts will be copied to $T32SYS folder.

Edit $T32SYS/user_config.cmm to match your particular build and debug environment

3.3.2.	Windows Machine
I suppose that you've installed Trace32 SW to C:\T32.
1)	Make directory C:\T32\tegra
2)	Copy all files in ./vendor/nvidia/tegra/core/mobile_linux/lauterbachscripts to C:\T32\tegra
3)	Copy C:\T32\tegra\user_config.cmm file to C:\T32

3.4.	Setup Build/Scripts environtments
3.4.1.	Linux Machine case
export TOP=~/{your ANDROID TOP folder}
source build/envsetup.sh
setpaths
choosecombo 1 {target board} 3

3.4.2.	Windows Machine case
3.4.2.1.	Samba setup
1)	Setup samba daemon in your linux machine
2)	Create network drive for your android source tree in your windows machine

3.4.2.2.	Configure envtegra.bat
1)	Open C:\T32\tegra\envtegra.bat
2)	Setup or edit environment variables for yours
set LNXHOME=/home/joshuac/    ; your home directory in linux machine.
                                your debugging source is in your home directory.
set SMBDRIVE=X:/              ; network drive for your source tree of linux machine
set T32SYS=C:\T32             ; directory where you installed Trace32 (windows)
set TOP=X:\R14.TOT            ; android top source directory path in network drive
set TARGET_PRODUCT=whistler   ; your target product name
set TARGET_BUILD_TYPE=release

3.4.2.3.	Configure user_config.cmm
1)	Open C:\T32\user_config.cmm
2)	Change path information for scripts
    from
        path + &TEGRA_TOP/core/mobile_linux/lauterbachscripts
        path + &TEGRA_TOP/core/mobile_linux/lauterbachscripts/&(TARGET_SOC)
    to
        path + &T32SYS/tegra
        path + &T32SYS/tegra/&(TARGET_SOC)

3)	If you changed TARGET_PRODUCT name, please add condition check routine and
    set TARGET_SOC and TARGET_BOARD variables.

    ELSE IF "&TARGET_PRODUCT"=="enterprise"
    (
      &TARGET_BOARD="enterprise"
    )
    ELSE IF "&TARGET_PRODUCT"=="YOURS"
    (
      &TARGET_BOARD="YOURS"
    )
    ELSE
    (
      PRINT "TARGET_PRODUCT=&(TARGET_PRODUCT) is not supported"
      STOP
    )

    ... skipped ...

    ELSE IF "&TARGET_BOARD"=="enterprise"
    (
      &TARGET_SOC="t30"
      &TARGET_CONFIG="NONE"  ; Debugger loading not supported
      &QUERY_CORE_COUNT="TRUE"
    )
    ELSE IF "&TARGET_BOARD"=="YOURS"
    (
      &TARGET_SOC="ap20 or t30"
      &QUERY_CORE_COUNT="TRUE"	; t30 only
    )
    ELSE
    (
      PRINT "TARGET_BOARD=&TARGET_BOARD not supported"
      STOP
    )

4.	Debugging with RAMdump scripts
4.1.	Getting RAM dump image (Linux PC)
Connect Trace32 hardware to USB port of Linux PC

Change directory to $T32SYS
; cd $T32SYS (i.e. /home/[username]/t32)

Execute CPU debugging command
; t32cpu &

If you run t32cpu, you can see several buttons on toolbar.
+--------+---------------------------------------------------+---------------------------+
| Button | Description	                                     | Script                    |
+--------+---------------------------------------------------+---------------------------+
| FS     | Load fastboot into SDRAM script[note 1]           | cpu_boot_sdram.cmm        |
| FB     | Run fastboot with image already loaded into SDRAM | cpu_boot_sdram_noload.cmm |
| AT     | Attach to CPU but don't do anything else          | cpu_attach.cmm            |
| AB     | Attach to CPU and load fastboot symbols[note 2]   | cpu_boot_attach.cmm       |
| AK     | Attach to CPU and load kernel symbols             | cpu_kernel_attach.cmm     |
| KB     | Load kernel image via JTAG backdoor loader        | cpu_kernel_load.cmm       |
| CU     | Attach to CPU in uni-processor mode[note 3]       | cpu_up_attach.cmm         |
| CM     | Attach to CPU in multi-processor mode[note 4]     | cpu_mp_attach.cmm         |
| Cn     | Select CPU n[note 5]                              | cpu_select.cmm            |
| RD     | RAM dump                                          | save_ramdump.cmm          |
+--------+---------------------------------------------------+---------------------------+
Notes
[1] Avoid unless you know what you're doing.
[2] If lauterbach fails to reliably attach to the CPU in the bootloader, try changing the system jtagclock setting from rtck to some other value (e.g. 10.0MHz) inside the jtag setup scripts (e.g. apxx_cpu_jtag_setup.cmm)
[3] Default mode - SMP debugging not allowed.
[4] Required for SMP debugging.
[5] Not allowed on AP15/16. Requires CM script to have been previously called.

Procedure for RAMdump
1)	Press [AK] button on toolbar to load symbol
2)	Press [CM] button on toolbar to attach multi core
    ; In case of T114/Tegra3, we should select 4 not (A)uto for core number
3)	Do BREAK in menu or toolbar to stop target operation
4)	Press [RD] to dump RAM
    ; When you dump register information, scripts check automatically if current core is
      online or not with referring to cpu_online_bits symbol value of linux kernel.

Several dump files are saved in $T32SYS folder. (ex. /home/[username]/t32)
+----------------------------------------+------------------------------+
| Saved file                             | Description                  |
+----------------------------------------+------------------------------+
| $(TARGET_SOC)_$(TARGET_BOARD)_DUMP.bin | RAM dump file                |
|                                        | ex1) ap20_whistler_DUMP.bn   |
|                                        | ex2) t30_enterprise_DUMP.bin |
|                                        | ex3) t11x_pluto_DUMP.bin     |
+----------------------------------------+------------------------------+
| MMU_COREn.cmm                          | MMU registers dump file      |
+----------------------------------------+------------------------------+
| REG_COREn.cmm                          | CPU registers dump file      |
+----------------------------------------+------------------------------+

RAMdump using Trace32 takes long time. It'll take about 90 mins for 1GB RAMdump.
If you want to change RAMdump size, please modify below file.
vendor/nvidia/tegra/core/mobile_linux/lauterbachscripts/ramdump_var.cmm
You can find &RAM_DUMP_SIZE definition in that.
Default RAMdump size of T114/Tegra3 is 1GB and Tegra2 is 512MB.

4.2.	Getting RAM dump image (Windows PC)
1)	Connect Trace32 hardware to USB port of Windows PC
2)	Execute command prompt console
3)	cd C:\T32\tegra
4)	execute envtegra.bat file
5)	execute t32cpu.bat file
6)	Please refer to description of Linux machine case for remains
If you want to change RAM_DUMP_SIZE, please modify C:\T32\tegra\ramdump_var.cmm.
All dump files will be saved in C:\T32\tegra.

4.3.	Debugging with RAMdump image (Linux PC)

When you debug with RAMdump image, you don't need Trace32 HW body,
because It's using Trace32 simulator mode.

Change directory to $T32SYS
; cd $T32SYS

Execute CPU debugging command
; t32ramdump &

1)	Press [RD] button from toolbar of Trace32 window

2)	Load Vmlinux ELF file
    ; select vmlinux file

3)	Load RAMdump binary (it's in $T32SYS folder)
    ; it must be $(TARGET_SOC)_$(TARGET_BOARD)_DUMP.bin
4)	Now, you'll see debugging windows.
You can change core with [C0]..[C3] buttons on toolbar.
If you want default window setting for debugging, please run "do ramdump_winset".

4.4.	Debugging with RAMdump image (Windows PC)
1)	When we use T32 simulator, we don't need Trace32 HW connection.
2)	If you run envtegra.bat already, jump to (6)
3)	Execute command prompt console
4)	cd C:\T32\tegra
5)	execute envtegra.bat file
6)	execute t32ramdump.bat file
7)	Please refer to description of Linux machine case for remains
