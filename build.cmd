@echo off
rem Builds RoomHome.exe for x64, x86 and ARM64, then the installer.
rem   build.cmd             exes + portable copies + installer
rem   build.cmd noinstall   exes only
rem   build.cmd x64         one architecture only (x64 / x86 / arm64), no installer
rem Needs Visual Studio 2022 or later with the C++ desktop workload and the x64, x86 and ARM64
rem build tools (found with vswhere; or set VCVARS to vcvarsall.bat), and NSIS 3 for the installer
rem (or set MAKENSIS). Works on an x64 or an ARM64 Windows build machine.
setlocal
set ROOT=%~dp0
set VER=1.2.0
if not defined MAKENSIS set "MAKENSIS=%ProgramFiles(x86)%\NSIS\makensis.exe"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if defined VCVARS goto :havevc
rem not -latest: that can pick another product on the same installer (e.g. SQL Server Management Studio)
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -all -prerelease -products * -sort -find VC\Auxiliary\Build\vcvarsall.bat`) do if not defined VCVARS set "VCVARS=%%i"
:havevc
if not exist "%VCVARS%" (
  echo vcvarsall.bat not found. Install Visual Studio with the C++ desktop workload, or set VCVARS.
  exit /b 1
)

rem vcvarsall host_target pairs for this machine
set HOSTARCH=%PROCESSOR_ARCHITECTURE%
if defined PROCESSOR_ARCHITEW6432 set HOSTARCH=%PROCESSOR_ARCHITEW6432%
set VC_X64=amd64
set VC_X86=amd64_x86
set VC_A64=amd64_arm64
if /i "%HOSTARCH%"=="ARM64" set VC_X64=arm64_amd64
if /i "%HOSTARCH%"=="ARM64" set VC_X86=arm64_x86
if /i "%HOSTARCH%"=="ARM64" set VC_A64=arm64

if not exist "%ROOT%res\RoomHome.ico" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%ROOT%tools\make-icon.ps1" || exit /b 1
)

if /i "%1"=="x64"   ( call :build x64 %VC_X64% & exit /b )
if /i "%1"=="x86"   ( call :build x86 %VC_X86% & exit /b )
if /i "%1"=="arm64" ( call :build arm64 %VC_A64% & exit /b )

call :build x64 %VC_X64% || exit /b 1
call :build x86 %VC_X86% || exit /b 1
call :build arm64 %VC_A64% || exit /b 1
if /i "%1"=="noinstall" exit /b 0

if not exist "%ROOT%dist" mkdir "%ROOT%dist"
copy /y "%ROOT%build\x64\RoomHome.exe" "%ROOT%dist\RoomHome-%VER%-x64.exe" >nul || exit /b 1
copy /y "%ROOT%build\x86\RoomHome.exe" "%ROOT%dist\RoomHome-%VER%-x86.exe" >nul || exit /b 1
copy /y "%ROOT%build\arm64\RoomHome.exe" "%ROOT%dist\RoomHome-%VER%-arm64.exe" >nul || exit /b 1
"%MAKENSIS%" /V2 /DVERSION=%VER% "%ROOT%installer\RoomHome.nsi" || exit /b 1
echo.
echo Done. Files are in %ROOT%dist
exit /b 0

:build
setlocal
call "%VCVARS%" %2 >nul || exit /b 1
set OUT=%ROOT%build\%1
if not exist "%OUT%" mkdir "%OUT%"
rc /nologo /fo "%OUT%\RoomHome.res" "%ROOT%src\RoomHome.rc" || exit /b 1
cl /nologo /std:c++20 /O2 /MT /W4 /EHsc /permissive- /utf-8 /MP /DUNICODE /D_UNICODE /DNDEBUG ^
   /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /Fo"%OUT%\\" ^
   "%ROOT%src\main.cpp" "%ROOT%src\home.cpp" "%ROOT%src\sheets.cpp" "%ROOT%src\widgets.cpp" ^
   "%ROOT%src\ui.cpp" "%ROOT%src\gfx.cpp" "%ROOT%src\system.cpp" "%ROOT%src\config.cpp" ^
   "%ROOT%src\json.cpp" "%ROOT%src\util.cpp" "%ROOT%src\lang.cpp" "%OUT%\RoomHome.res" ^
   /link /SUBSYSTEM:WINDOWS /MANIFEST:NO /OUT:"%OUT%\RoomHome.exe" ^
   d2d1.lib dwrite.lib windowscodecs.lib wlanapi.lib iphlpapi.lib ws2_32.lib ole32.lib oleaut32.lib uuid.lib ^
   shell32.lib shlwapi.lib user32.lib gdi32.lib advapi32.lib comctl32.lib dxguid.lib dwmapi.lib || exit /b 1
echo built %OUT%\RoomHome.exe
exit /b 0
