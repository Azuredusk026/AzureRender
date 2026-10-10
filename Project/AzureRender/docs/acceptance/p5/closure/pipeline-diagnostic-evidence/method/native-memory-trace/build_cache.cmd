@echo off
call "D:\Visual Studio\2022\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.38
if errorlevel 1 exit /b 1
"D:\Visual Studio\2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -S build/p5-p6-closure-2026-10-10/native-memory-trace -B build/p5-p6-closure-2026-10-10/native-memory-trace/compiled-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 2
"D:\Visual Studio\2022\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" --build build/p5-p6-closure-2026-10-10/native-memory-trace/compiled-ninja --target AzureCacheShim
exit /b %errorlevel%
