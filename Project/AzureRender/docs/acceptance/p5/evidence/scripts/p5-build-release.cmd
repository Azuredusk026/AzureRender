@echo off
call "D:\Visual Studio\2022\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul
set "VCPKG_ROOT=C:\Users\23587\Tools\vcpkg"
set "PATH=C:\Users\23587\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe;%PATH%"
python tools/build_provenance.py build --build-dir build/ninja-msvc-release --config Release
