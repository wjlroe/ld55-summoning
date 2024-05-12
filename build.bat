@echo off
setlocal

set BUILD_PATH=%~dp0

SET /A errno=0

set build_message=BUILD_PATH is %BUILD_PATH%
echo %build_message%
cd %BUILD_PATH%

set build_dir="build"
IF NOT EXIST %build_dir% mkdir %build_dir%

pushd build
del -q *.pdb *.obj *.ilk *.res
popd

REM Debug build
C:\dev\odin\dev-master\odin build src ^
    -out:build/summoning_debug.exe ^
    -build-mode:exe ^
    -subsystem:console ^
    -debug ^
    -show-timings || goto :error

goto :end

:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%

:end

endlocal
