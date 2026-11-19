@echo off
call "D:/Tools/VS2022BuildTools/Common7/Tools/VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
set "EDITOR_ENGINE=D:/Projects/FYP-worktrees/editor-interaction/Project/AzureRender"
set "EDITOR_BUILD=%EDITOR_ENGINE%/build/ninja-msvc-release"
set "EDITOR_CMAKE=D:/Tools/VS2022BuildTools/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
set "EDITOR_PYTHON=D:/Projects/FYP/Project/AzureRender/build/reinstall-environment/venv/Scripts/python.exe"
set "VULKAN_SDK=D:/Tools/VulkanSDK/1.4.350.0"
set "PATH=%VULKAN_SDK%/Bin;%PATH%"
"%EDITOR_CMAKE%/cmake.exe" --build "%EDITOR_BUILD%" --target AzureRender AzureSelectionServiceTests AzureEditorCreationTests AzureEditorNodeAdditionTests AzureRenderEditorSessionTests --parallel 8 > "%~dp0release-build.log" 2>&1
if errorlevel 1 exit /b %errorlevel%
"%EDITOR_CMAKE%/ctest.exe" --test-dir "%EDITOR_BUILD%" --output-on-failure --no-tests=error -R "^(AzureEditor\.(NodeAddition|Creation|SelectionService)|AzureRender\.EditorSession)$" --output-log "%~dp0release-ctest.log"
if errorlevel 1 exit /b %errorlevel%
"%EDITOR_PYTHON%" -B -X utf8 "%EDITOR_ENGINE%/tools/test_editor_selection.py" --executable "%EDITOR_BUILD%/AzureRender.exe" --output "%~dp0candidate-selection" > "%~dp0candidate-selection.log" 2>&1
if errorlevel 1 exit /b %errorlevel%
"%EDITOR_PYTHON%" -B -X utf8 "%EDITOR_ENGINE%/tools/test_editor_node_addition.py" --executable "%EDITOR_BUILD%/AzureRender.exe" --output "%~dp0candidate-creation" > "%~dp0candidate-creation.log" 2>&1
exit /b %errorlevel%
