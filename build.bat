@echo off
setlocal

set start=%time%
set BUILD_PATH=%~dp0

IF NOT DEFINED build32 (SET build32=false)
echo build32 is %build32%

SET /A errno=0

set vclocations="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build"
set vclocations=%vclocations%;"C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build"
set vclocations=%vclocations%;"C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build"

(for %%f in (%vclocations%) do (
    set vc_location="%%f"
    if exist "%%f" (GOTO :foundLocation)
))

:foundLocation

if not exist "%vc_location%" (
    echo Can't find Visual Studio location
    exit /b 1
)

echo VS Location: %vc_location%

pushd "%vc_location%"
IF %build32% == true (
  echo Building 32bit
  call vcvarsall x86
) ELSE (
  echo Building 64bit
  call vcvarsall x64
)
popd

set build_message=BUILD_PATH is %BUILD_PATH%
echo %build_message%
cd %BUILD_PATH%

IF %build32% == true (
  set build_dir="build32"
  set build_flags="/MT"
) ELSE (
  set build_dir="build"
  set build_flags=""
)
IF NOT EXIST %build_dir% mkdir %build_dir%
pushd %build_dir%

cp ..\vendor\SDL2-2.30.2\lib\x64\SDL2.dll .
cp ..\vendor\SDL2_image-2.8.2\lib\x64\SDL2_image.dll .
cp ..\vendor\SDL2_ttf-2.22.0\lib\x64\SDL2_ttf.dll .

cl /std:c11 -FC -Zc:strictStrings -Zi -EHsc -diagnostics:column %build_flags:"=% ^
 -Fe:summoning.exe ..\main.c ^
 -I ..\vendor\SDL2-2.30.2\include ^
 -I ..\vendor\SDL2-2.30.2\include\SDL2 ^
 -I ..\vendor\SDL2_image-2.8.2\include ^
 -I ..\vendor\SDL2_ttf-2.22.0\include ^
 /link /DEBUG:FULL ..\vendor\SDL2-2.30.2\lib\x64\SDL2.lib ^
 ..\vendor\SDL2_image-2.8.2\lib\x64\SDL2_image.lib ^
 ..\vendor\SDL2_ttf-2.22.0\lib\x64\SDL2_ttf.lib ^
 opengl32.lib glu32.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS
IF %ERRORLEVEL% NEQ 0 SET /A errno=%ERRORLEVEL%
popd

set end=%time%

set options="tokens=1-4 delims=:.,"
for /f %options% %%a in ("%start%") do set start_h=%%a&set /a start_m=100%%b %% 100&set /a start_s=100%%c %% 100&set /a start_ms=100%%d %% 100
for /f %options% %%a in ("%end%") do set end_h=%%a&set /a end_m=100%%b %% 100&set /a end_s=100%%c %% 100&set /a end_ms=100%%d %% 100

set /a hours=%end_h%-%start_h%
set /a mins=%end_m%-%start_m%
set /a secs=%end_s%-%start_s%
set /a ms=%end_ms%-%start_ms%
if %ms% lss 0 set /a secs = %secs% - 1 & set /a ms = 100%ms%
if %secs% lss 0 set /a mins = %mins% - 1 & set /a secs = 60%secs%
if %mins% lss 0 set /a hours = %hours% - 1 & set /a mins = 60%mins%
if %hours% lss 0 set /a hours = 24%hours%
if 1%ms% lss 100 set ms=0%ms%

:: Mission accomplished
set /a totalsecs = %hours%*3600 + %mins%*60 + %secs%
echo compile took (%totalsecs%.%ms%s total)

if %errno% neq 0 EXIT /B %errno%
