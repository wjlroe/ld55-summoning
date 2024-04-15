@echo off
setlocal

set BUILD_PATH=%~dp0

SET /A errno=0

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set build_message=BUILD_PATH is %BUILD_PATH%
echo %build_message%
cd %BUILD_PATH%

rc /r icon.rc

set build_dir="build"
IF NOT EXIST %build_dir% mkdir %build_dir%
pushd %build_dir%

cp ..\vendor\SDL2-2.30.2\lib\x64\SDL2.dll .
cp ..\vendor\SDL2_image-2.8.2\lib\x64\SDL2_image.dll .
cp ..\vendor\SDL2_ttf-2.22.0\lib\x64\SDL2_ttf.dll .
mv ..\icon.res .
cvtres -machine:x64 -out:icon.obj icon.res

set includes=-I ..\vendor\SDL2-2.30.2\include ^
 -I ..\vendor\SDL2-2.30.2\include\SDL2 ^
 -I ..\vendor\SDL2_image-2.8.2\include ^
 -I ..\vendor\SDL2_ttf-2.22.0\include ^
 -I ..\vendor\SDL2_gfx

set links=..\vendor\SDL2-2.30.2\lib\x64\SDL2.lib ^
 ..\vendor\SDL2_image-2.8.2\lib\x64\SDL2_image.lib ^
 ..\vendor\SDL2_ttf-2.22.0\lib\x64\SDL2_ttf.lib ^
 opengl32.lib glu32.lib user32.lib gdi32.lib

echo Debug build
cl /std:c11 -FC -Zc:strictStrings -Zi -diagnostics:caret /nologo /DDEBUG ^
 -Fe:summoning_debug.exe ..\main.c icon.obj ^
 %includes% ^
 /link /DEBUG:FULL %links% ^
 /subsystem:console
IF %ERRORLEVEL% NEQ 0 SET /A errno=%ERRORLEVEL%

echo Release build
cl /std:c11 -FC -Zc:strictStrings -diagnostics:caret /nologo ^
 -Fe:summoning.exe ..\main.c icon.obj ^
 %includes% ^
 /link %links% ^
 /subsystem:windows
IF %ERRORLEVEL% NEQ 0 SET /A errno=%ERRORLEVEL%

popd

if %errno% neq 0 EXIT /B %errno%
