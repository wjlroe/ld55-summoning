@echo off
setlocal

set BUILD_PATH=%~dp0

SET /A errno=0

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set build_message=BUILD_PATH is %BUILD_PATH%
echo %build_message%
cd %BUILD_PATH%

rc /r resources.rc

set build_dir="build"
IF NOT EXIST %build_dir% mkdir %build_dir%
pushd %build_dir%

rm *.pdb *.obj *.ilk

cp ..\vendor\SDL2-2.30.2\lib\x64\SDL2.dll .
cp ..\vendor\SDL2_image-2.8.2\lib\x64\SDL2_image.dll .
cp ..\vendor\SDL2_ttf-2.22.0\lib\x64\SDL2_ttf.dll .
mv ..\resources.res .
cvtres -machine:x64 -out:resources.obj resources.res

set includes=-I. ^
 -I..\vendor\stb ^
 -I..\vendor\SDL2-2.30.2\include ^
 -I..\vendor\SDL2-2.30.2\include\SDL2 ^
 -I..\vendor\SDL2_image-2.8.2\include ^
 -I..\vendor\SDL2_ttf-2.22.0\include ^
 -I..\vendor\SDL2_gfx

set links=..\vendor\SDL2-2.30.2\lib\x64\SDL2.lib ^
 ..\vendor\SDL2_image-2.8.2\lib\x64\SDL2_image.lib ^
 ..\vendor\SDL2_ttf-2.22.0\lib\x64\SDL2_ttf.lib ^
 glu32.lib user32.lib gdi32.lib

echo Generate resources
cl /std:c11 -Gm- -FC -Zc:strictStrings -Zi -diagnostics:caret /nologo /DDEBUG ^
 -Fe:generate_resources.exe ..\src\generate_resources.c ^
 /link /DEBUG:FULL -INCREMENTAL:NO ^
 /subsystem:console || goto :error

.\generate_resources.exe || goto :error

echo Debug build
cl /std:c11 -Gm- -FC -Zc:strictStrings -Zi -diagnostics:caret /nologo /DDEBUG ^
 -Fe:summoning_debug.exe ..\src\main.c resources.obj ^
 %includes% ^
 /link /DEBUG:FULL -INCREMENTAL:NO %links% ^
 /subsystem:windows || goto :error

echo Release build
cl /std:c11 -Gm- -FC -Zc:strictStrings -diagnostics:caret /nologo ^
 -Fe:summoning.exe ..\src\main.c resources.obj ^
 %includes% ^
 /link -INCREMENTAL:NO %links% ^
 /subsystem:windows || goto :error

popd

goto :end

:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%

:end

endlocal