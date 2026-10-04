@echo off
setlocal EnableExtensions EnableDelayedExpansion
rem Sets up an MSVC + Ninja environment for local development.
rem
rem Ninja tracks header dependencies by matching the /showIncludes prefix that
rem CMake records at configure time against cl's actual output bytes. When the
rem recorded prefix diverges (localized toolchain + code page mismatch), header
rem edits stop triggering rebuilds. CMake verifies the match at configure time;
rem if it warns, run "python tools/fix_msvc_deps_prefix.py <build-dir>" and
rem configure again. VSLANG=1033 additionally selects English diagnostics on
rem installations that have the English language pack.
rem
rem Usage: tools\msvc_env.bat <command> [args...]
set VSLANG=1033
set "_AZR_REQUESTED_VCPKG=%VCPKG_ROOT%"
if defined VSINSTALLDIR (
    set "_AZR_VS_INSTALL=%VSINSTALLDIR%"
) else (
    set "_AZR_VSWHERE=!ProgramFiles(x86)!\Microsoft Visual Studio\Installer\vswhere.exe"
    if not exist "!_AZR_VSWHERE!" (
        echo Visual Studio Installer vswhere.exe was not found. 1>&2
        endlocal & exit /b 1
    )
    for /f "usebackq tokens=*" %%i in (`"!_AZR_VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "_AZR_VS_INSTALL=%%i"
)
if not defined _AZR_VS_INSTALL (
    echo Visual Studio with the MSVC x64 build tools was not found. 1>&2
    endlocal & exit /b 1
)
set "_AZR_VSDEVCMD=%_AZR_VS_INSTALL%\Common7\Tools\VsDevCmd.bat"
if not exist "!_AZR_VSDEVCMD!" (
    echo Visual Studio developer environment script was not found: "!_AZR_VSDEVCMD!" 1>&2
    endlocal & exit /b 1
)
call "!_AZR_VSDEVCMD!" -arch=amd64 -host_arch=amd64 >nul
if errorlevel 1 (
    echo Failed to initialize the Visual Studio developer environment. 1>&2
    endlocal & exit /b 1
)
if defined _AZR_REQUESTED_VCPKG set "VCPKG_ROOT=!_AZR_REQUESTED_VCPKG!"

if not defined VULKAN_SDK (
    echo Set VULKAN_SDK to the installed Vulkan SDK directory. 1>&2
    endlocal & exit /b 1
)
if not exist "!VULKAN_SDK!\Include\vulkan\vulkan.h" (
    echo Vulkan headers were not found under VULKAN_SDK: "!VULKAN_SDK!" 1>&2
    endlocal & exit /b 1
)
if not defined VCPKG_ROOT (
    echo Set VCPKG_ROOT to the vcpkg checkout directory. 1>&2
    endlocal & exit /b 1
)
if not exist "!VCPKG_ROOT!\scripts\buildsystems\vcpkg.cmake" (
    echo vcpkg CMake toolchain was not found under VCPKG_ROOT: "!VCPKG_ROOT!" 1>&2
    endlocal & exit /b 1
)

where cmake.exe >nul 2>nul
if errorlevel 1 if exist "!_AZR_VS_INSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "PATH=!_AZR_VS_INSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;!PATH!"
)
where ninja.exe >nul 2>nul
if errorlevel 1 if exist "!_AZR_VS_INSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" (
    set "PATH=!_AZR_VS_INSTALL!\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;!PATH!"
)
where cmake.exe >nul 2>nul || (
    echo CMake was not found on PATH or in the Visual Studio installation. 1>&2
    endlocal & exit /b 1
)
where ninja.exe >nul 2>nul || (
    echo Ninja was not found on PATH or in the Visual Studio installation. 1>&2
    endlocal & exit /b 1
)
if "%~1"=="" (
    echo Usage: tools\msvc_env.bat ^<command^> [args...] 1>&2
    endlocal & exit /b 2
)
%*
set "_AZR_EXIT=%ERRORLEVEL%"
endlocal & exit /b %_AZR_EXIT%
