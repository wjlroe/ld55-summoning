@echo off
setlocal

set BUILD_PATH=%~dp0

SET /A errno=0

call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64

set build_message=BUILD_PATH is %BUILD_PATH%
echo %build_message%
cd %BUILD_PATH%

set build_dir="build"
IF NOT EXIST %build_dir% mkdir %build_dir%
pushd %build_dir%

del -q *.pdb *.obj *.ilk *.res

copy ..\vendor\SDL2-2.30.2\lib\x64\SDL2.dll .

rc -nologo -r -fo icon.res ..\icon.rc
cvtres -nologo -machine:x64 -out:icon.obj icon.res

rc -nologo -r -fo resources.res ..\resources.rc
cvtres -nologo -machine:x64 -out:resources.obj resources.res

set includes=-I. ^
 -I..\vendor\stb ^
 -I..\vendor\SDL2-2.30.2\include ^
 -I..\vendor\SDL2-2.30.2\include\SDL2

set links=..\vendor\SDL2-2.30.2\lib\x64\SDL2.lib ^
 glu32.lib user32.lib gdi32.lib

echo Compile generate resources
cl -std:c11 -Gm- -FC -Zc:strictStrings -Zi -diagnostics:caret -nologo -DDEBUG -DDEBUG_STDOUT ^
 -Fe:generate_resources.exe ..\src\generate_resources.c ^
 -link -DEBUG:FULL -INCREMENTAL:NO ^
 -subsystem:console || goto :error

echo Generate resources
.\generate_resources.exe || goto :error

echo Debug build
cl -std:c11 -Gm- -FC -Zc:strictStrings -Zi -diagnostics:caret -nologo -DDEBUG -DDEBUG_STDOUT ^
 -Fe:summoning_debug.exe ..\src\main.c icon.obj ^
 %includes% ^
 -link -DEBUG:FULL -INCREMENTAL:NO %links% ^
 -subsystem:console || goto :error

echo Release build
cl -std:c11 -Gm- -FC -Zc:strictStrings -diagnostics:caret -nologo ^
 -Fe:summoning.exe ..\src\main.c resources.obj ^
 %includes% ^
 -link -INCREMENTAL:NO %links% ^
 -subsystem:windows || goto :error

popd

goto :end

:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%

:end

endlocal