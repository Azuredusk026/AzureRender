@echo off
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
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul
set VULKAN_SDK=C:\VulkanSDK\1.4.357.0
set VCPKG_ROOT=C:\vcpkg
set PATH=C:\Program Files\CMake\bin;C:\Users\wuchenfeng\AppData\Local\Microsoft\WinGet\Links;%VULKAN_SDK%\Bin;%PATH%
%*
