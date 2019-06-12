@echo off
setlocal
REM ##############################################
REM     In order to build nvflash, you  need the WDK version 6001 (for Vista) or newer.  
REM     See from http://www.microsoft.com/whdc/DevTools/WDK/WDKpkg.mspx for more info.
REM     Note that the older Windows XP/2003 DDK will not work because it doesn't contain winusb.h etc.
REM ##############################################

REM ##############################################
REM     Change WDK_ROOT_DIR and DEVENV_COMMAND below to match your environment
REM ##############################################

REM   Change WDK_ROOT_DIR to point to your WDK installation directory.  Your build will fail if this isn't set.
set WDK_ROOT_DIR=C:\WinDDK\7600.16385.1
REM Change TARGET_OUT_HEADERS to ${OUT}\obj\include directory.
set TARGET_OUT_HEADERS=Z:\mocha\out\target\product\mocha\obj\include
REM Now that WDK_ROOT_DIR is set, launch Visual Studio and do a build.

REM Uncomment the line below to launch the Visual Studio 2005 IDE.  Once the IDE has lauched, do a build.
REM set DEVENV_COMMAND="C:\Program Files\Microsoft Visual Studio 8\Common7\IDE\devenv.exe"
set DEVENV_COMMAND="C:\Program Files (x86)\Microsoft Visual Studio 8\Common7\IDE\devenv.exe"
REM Uncomment the line below to build using VS2005 without launching the Visual Studio IDE.
REM set DEVENV_COMMAND="C:\Program Files\Microsoft Visual Studio 8\Common7\IDE\devenv.com" /build Release
REM Uncomment the line below to use Visual Studio 2008 instead of VS 2005
REM set DEVENV_COMMAND="C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE\devenv.exe"

REM ##############################################
REM  You shouldn't need to edit the lines below
REM ##############################################
REM Set working directory to the same directory as this BAT file, 
REM then launch Visual Studio 2005 with the nvflash solution file. 

REM The devenv.exe process will inherit the environment variables from
REM this command prompt session, so the WDK_ROOT_DIR environmen variable
REM will be set.

pushd "%~dp0"
%DEVENV_COMMAND% nvflash.sln 
popd

