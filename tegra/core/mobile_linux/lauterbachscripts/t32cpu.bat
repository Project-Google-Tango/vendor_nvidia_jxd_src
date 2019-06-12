@ECHO OFF
REM ---------------------------------------------------------------
REM  Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
REM
REM This file is for user who use Windows OS based Trace32
REM with Samba connection for source browsing
REM This script is used to save RAMdump data with Trace32.
REM ---------------------------------------------------------------
REM NOTE :
REM If t32marm.exe is installed in other directory,
REM you should change full path of t32marm below.
REM ---------------------------------------------------------------

set T32_INSTANCE=cpu
%T32SYS%\bin\windows64\t32marm.exe -c %T32TEGRA%\config_cpu_win.t32,%T32TEGRA%\t32.cmm
