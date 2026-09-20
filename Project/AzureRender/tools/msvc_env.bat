@echo off
rem Sets up an MSVC + Ninja environment for local development.
rem
rem VSLANG=1033 forces English compiler diagnostics. Ninja parses the localized
rem "/showIncludes" prefix to track header dependencies; on a localized
rem toolchain that prefix can be unparseable, in which case editing a header
rem does not trigger a rebuild and a stale object silently crashes at runtime.
rem
rem Usage: tools\msvc_env.bat <command> [args...]
set VSLANG=1033
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 -host_arch=amd64 >nul
set VULKAN_SDK=C:\VulkanSDK\1.4.357.0
set VCPKG_ROOT=C:\vcpkg
set PATH=C:\Program Files\CMake\bin;C:\Users\wuchenfeng\AppData\Local\Microsoft\WinGet\Links;%VULKAN_SDK%\Bin;%PATH%
%*
