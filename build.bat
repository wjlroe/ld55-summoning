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
del *.pdb *.obj *.ilk *.res
popd

set build_debug=yes

if "%1"=="release" (
    set build_debug=no
    set build_release=yes
)

if "%1"=="all" (
    set build_debug=yes
    set build_release=yes
)

if "%build_debug%"=="yes" (
    echo Debug build
    C:\dev\odin\dev-master\odin build src ^
        -out:build/summoning_debug.exe ^
        -build-mode:exe ^
        -subsystem:console ^
        -define:RAYLIB_SHARED=true ^
        -debug ^
        -resource:icon.rc ^
        -show-timings || goto :error
)

if "%build_release%"=="yes" (
    REM TODO: work out how to statically link this
    echo Release build
    C:\dev\odin\dev-master\odin build src ^
        -out:build/summoning.exe ^
        -build-mode:exe ^
        -subsystem:windows ^
        -o:speed ^
        -disable-assert ^
        -resource:resources.rc ^
        -show-timings || goto :error
)

goto :end

:error
echo Failed with error #%errorlevel%.
exit /b %errorlevel%

:end

endlocal
