cmake_minimum_required(VERSION 3.20)

project(
    AzureRender
    VERSION 0.1.0
    DESCRIPTION "Portfolio-first Vulkan stylized renderer"
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Dear ImGui integration.
#
# Windows MinGW builds cannot link the vcpkg `x64-windows` (MSVC) imgui static
# library because the C++ name mangling schemes are incompatible. The project
# therefore compiles the vendored imgui sources under `third_party/imgui` so the
# library always uses the same ABI as the project compiler. When the vendored
# sources are absent, the vcpkg imgui package is used as a fallback.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp")
    add_library(azure_imgui STATIC
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/backends/imgui_impl_glfw.cpp
        third_party/imgui/backends/imgui_impl_vulkan.cpp
    )
    target_include_directories(
        azure_imgui
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui"
            "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends"
    )
    target_compile_features(azure_imgui PUBLIC cxx_std_17)
    target_compile_definitions(azure_imgui PUBLIC GLFW_INCLUDE_VULKAN)
    target_link_libraries(azure_imgui PUBLIC Vulkan::Vulkan glfw)
    add_library(imgui::imgui ALIAS azure_imgui)
else()
    find_package(imgui CONFIG QUIET)
    if(NOT TARGET imgui::imgui)
        find_path(IMGUI_INCLUDE_DIR imgui.h PATH_SUFFIXES imgui)
        find_library(IMGUI_LIBRARY NAMES imgui)
        if(IMGUI_INCLUDE_DIR AND IMGUI_LIBRARY)
            add_library(imgui::imgui UNKNOWN IMPORTED)
            find_library(IMGUI_STB_LIBRARY NAMES stb)
            set_target_properties(
                imgui::imgui
                PROPERTIES
                    IMPORTED_LOCATION "${IMGUI_LIBRARY}"
                    INTERFACE_INCLUDE_DIRECTORIES "${IMGUI_INCLUDE_DIR}"
            )
            if(IMGUI_STB_LIBRARY)
                set_property(
                    TARGET imgui::imgui APPEND PROPERTY
                    INTERFACE_LINK_LIBRARIES "${IMGUI_STB_LIBRARY}")
            endif()
        endif()
    endif()
endif()
find_path(TINYGLTF_INCLUDE_DIR tiny_gltf.h REQUIRED)
find_path(STB_INCLUDE_DIR stb_easy_font.h PATH_SUFFIXES stb REQUIRED)

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")

function(compile_shader source_file)
    get_filename_component(shader_name "${source_file}" NAME)
    set(shader_output "${SHADER_BINARY_DIR}/${shader_name}.spv")

    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_BINARY_DIR}"
        COMMAND Vulkan::glslc "${source_file}" -o "${shader_output}"
        DEPENDS "${source_file}"
        COMMENT "Compiling ${shader_name}"
        VERBATIM
    )

    set(COMPILED_SHADERS ${COMPILED_SHADERS} "${shader_output}" PARENT_SCOPE)
endfunction()

compile_shader("${SHADER_SOURCE_DIR}/mesh.vert")
compile_shader("${SHADER_SOURCE_DIR}/mesh.frag")
compile_shader("${SHADER_SOURCE_DIR}/outline.vert")
compile_shader("${SHADER_SOURCE_DIR}/outline.frag")
compile_shader("${SHADER_SOURCE_DIR}/background.vert")
compile_shader("${SHADER_SOURCE_DIR}/background.frag")
compile_shader("${SHADER_SOURCE_DIR}/shadow.vert")
compile_shader("${SHADER_SOURCE_DIR}/shadow.frag")
compile_shader("${SHADER_SOURCE_DIR}/inner_outline.vert")
compile_shader("${SHADER_SOURCE_DIR}/inner_outline.frag")
compile_shader("${SHADER_SOURCE_DIR}/hud.vert")
compile_shader("${SHADER_SOURCE_DIR}/hud.frag")
add_custom_target(AzureRenderShaders ALL DEPENDS ${COMPILED_SHADERS})

add_executable(
    AzureRender
    src/main.cpp
    src/app/AzureRenderOptions.hpp
    src/app/CommandLine.cpp
    src/app/CommandLine.hpp
    src/app/AzureRenderApp.cpp
    src/app/AzureRenderApp.hpp
    src/app/AzureRenderInternal.hpp
    src/app/AzureRenderCapture.cpp
    src/app/AzureRenderDescriptors.cpp
    src/app/AzureRenderFrame.cpp
    src/app/AzureRenderPipeline.cpp
    src/app/AzureRenderResources.cpp
    src/app/AzureRenderSupport.cpp
    src/assets/GltfLoader.cpp
    src/assets/GltfLoader.hpp
    src/editor/EditorCameraController.cpp
    src/editor/EditorCameraController.hpp
    src/editor/EditorContext.cpp
    src/editor/EditorContext.hpp
    src/editor/EditorSession.cpp
    src/editor/EditorSession.hpp
    src/editor/IEditorPanel.hpp
    src/editor/ImGuiEditorLayer.cpp
    src/editor/ImGuiEditorLayer.hpp
    src/editor/SceneModel.cpp
    src/editor/SceneModel.hpp
    src/diagnostics/GpuCapabilityReport.cpp
    src/diagnostics/GpuCapabilityReport.hpp
    src/diagnostics/RuntimeDiagnostics.cpp
    src/diagnostics/RuntimeDiagnostics.hpp
    src/resources/ResourceLocator.cpp
    src/resources/ResourceLocator.hpp
    src/extensions/ExtensionRegistry.hpp
    src/extensions/IAssetImporter.hpp
    src/extensions/IRenderFeature.hpp
    src/extensions/ISceneRenderer.hpp
    src/extensions/SceneType.hpp
    src/platform/GlfwFrontend.cpp
    src/platform/GlfwFrontend.hpp
    src/render/RendererCore.cpp
    src/render/RendererCore.hpp
    src/render/RenderSettings.cpp
    src/render/RenderSettings.hpp
    src/render/RenderContext.hpp
)

add_dependencies(AzureRender AzureRenderShaders)

if(BUILD_TESTING)
    add_executable(
        AzureRenderCommandLineTests
        tests/CommandLineTests.cpp
        src/app/CommandLine.cpp
        src/app/CommandLine.hpp
        src/app/AzureRenderOptions.hpp
    )
    target_include_directories(
        AzureRenderCommandLineTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(
        NAME AzureRender.CommandLine
        COMMAND AzureRenderCommandLineTests)
    add_executable(
        AzureRenderEditorCameraTests
        tests/EditorCameraControllerTests.cpp
        src/editor/EditorCameraController.cpp
        src/editor/EditorCameraController.hpp
    )
    target_include_directories(
        AzureRenderEditorCameraTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(
        NAME AzureRender.EditorCameraController
        COMMAND AzureRenderEditorCameraTests)
    add_executable(
        AzureRenderEditorSessionTests
        tests/EditorSessionTests.cpp
        src/editor/EditorContext.cpp
        src/editor/EditorContext.hpp
        src/editor/EditorSession.cpp
        src/editor/EditorSession.hpp
        src/editor/SceneModel.cpp
        src/editor/SceneModel.hpp
        src/render/RenderSettings.cpp
        src/render/RenderSettings.hpp
        src/diagnostics/RuntimeDiagnostics.cpp
        src/diagnostics/RuntimeDiagnostics.hpp
    )
    target_include_directories(
        AzureRenderEditorSessionTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(
        NAME AzureRender.EditorSession
        COMMAND AzureRenderEditorSessionTests)
    add_executable(
        AzureRenderEcsTests
        tests/EcsTests.cpp)
    target_include_directories(
        AzureRenderEcsTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(
        NAME AzureRender.Ecs
        COMMAND AzureRenderEcsTests)
    add_executable(
        AzureRenderPickMathTests
        tests/PickMathTests.cpp)
    target_include_directories(
        AzureRenderPickMathTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(
        AzureRenderPickMathTests PRIVATE Vulkan::Vulkan)
    add_test(
        NAME AzureRender.PickMath
        COMMAND AzureRenderPickMathTests)
    add_executable(
        AzureRenderSceneModelTests
        tests/SceneModelTests.cpp
        src/editor/SceneModel.cpp
        src/editor/SceneModel.hpp
        src/render/RenderSettings.cpp
        src/render/RenderSettings.hpp
    )
    target_include_directories(
        AzureRenderSceneModelTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(
        NAME AzureRender.SceneModel
        COMMAND AzureRenderSceneModelTests)
    add_executable(
        AzureRenderRuntimeDiagnosticsTests
        tests/RuntimeDiagnosticsTests.cpp
        src/diagnostics/RuntimeDiagnostics.cpp
        src/diagnostics/RuntimeDiagnostics.hpp
    )
    target_include_directories(
        AzureRenderRuntimeDiagnosticsTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(
        NAME AzureRender.RuntimeDiagnostics
        COMMAND AzureRenderRuntimeDiagnosticsTests)
    add_executable(
        AzureRenderGpuCapabilityReportTests
        tests/GpuCapabilityReportTests.cpp
        src/diagnostics/GpuCapabilityReport.cpp
        src/diagnostics/GpuCapabilityReport.hpp
        src/diagnostics/RuntimeDiagnostics.cpp
        src/diagnostics/RuntimeDiagnostics.hpp
    )
    target_include_directories(
        AzureRenderGpuCapabilityReportTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(
        AzureRenderGpuCapabilityReportTests PRIVATE
            Vulkan::Vulkan
            nlohmann_json::nlohmann_json)
    add_test(
        NAME AzureRender.GpuCapabilityReport
        COMMAND AzureRenderGpuCapabilityReportTests)
    add_executable(
        AzureRenderResourceLocatorTests
        tests/ResourceLocatorTests.cpp
        src/resources/ResourceLocator.cpp
        src/resources/ResourceLocator.hpp
        src/diagnostics/RuntimeDiagnostics.cpp
        src/diagnostics/RuntimeDiagnostics.hpp
    )
    target_include_directories(
        AzureRenderResourceLocatorTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(
        NAME AzureRender.ResourceLocator
        COMMAND AzureRenderResourceLocatorTests)
    add_executable(
        AzureRenderExtensionRegistryTests
        tests/ExtensionRegistryTests.cpp
    )
    target_include_directories(
        AzureRenderExtensionRegistryTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(
        AzureRenderExtensionRegistryTests PRIVATE Vulkan::Vulkan)
    add_test(
        NAME AzureRender.ExtensionRegistry
        COMMAND AzureRenderExtensionRegistryTests)
    add_test(
        NAME AzureRender.ReleaseGateMissingBuild
        COMMAND "${CMAKE_COMMAND}"
            -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}/missing-release-gate-build
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/run_release_gate.cmake)
    set_tests_properties(
        AzureRender.ReleaseGateMissingBuild PROPERTIES WILL_FAIL TRUE)
    add_test(
        NAME AzureRender.InstallManifestRoundTrip
        COMMAND "${CMAKE_COMMAND}"
            -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_install_manifest_roundtrip.cmake)
endif()

target_link_libraries(
    AzureRender
    PRIVATE
        Vulkan::Vulkan
        glfw
        nlohmann_json::nlohmann_json
)

if(TARGET imgui::imgui)
    target_link_libraries(AzureRender PRIVATE imgui::imgui)
    target_compile_definitions(AzureRender PRIVATE AZURERENDER_HAS_IMGUI=1)
else()
    message(WARNING
        "Dear ImGui package not found; editor UI backend is disabled. "
        "Use the vcpkg manifest for the Docking-enabled editor build.")
endif()

target_include_directories(
    AzureRender
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${TINYGLTF_INCLUDE_DIR}"
        "${STB_INCLUDE_DIR}"
)

target_compile_definitions(
    AzureRender
    PRIVATE
        GLFW_INCLUDE_VULKAN
        AZURERENDER_VERSION="${PROJECT_VERSION}-rc1"
        AZURERENDER_DEVELOPMENT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        $<$<CONFIG:Debug>:AZURERENDER_ENABLE_VALIDATION=1>
)

if(MSVC)
    target_compile_options(AzureRender PRIVATE /W4 /permissive-)
else()
    target_compile_options(
        AzureRender
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-missing-field-initializers
    )
endif()

if(WIN32)
    add_custom_command(
        TARGET AzureRender
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:glfw>"
                "$<TARGET_FILE_DIR:AzureRender>"
        COMMENT "Copying GLFW runtime"
    )
endif()

if(MINGW)
    get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(runtime_dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${MINGW_RUNTIME_DIR}/${runtime_dll}")
            add_custom_command(
                TARGET AzureRender
                POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${MINGW_RUNTIME_DIR}/${runtime_dll}"
                        "$<TARGET_FILE_DIR:AzureRender>"
                COMMENT "Copying ${runtime_dll}"
            )
        endif()
    endforeach()
endif()

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT AzureRender)

set_target_properties(AzureRender PROPERTIES INSTALL_RPATH "$ORIGIN")
install(TARGETS AzureRender RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
    DIRECTORY "${SHADER_BINARY_DIR}/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/shaders"
    FILES_MATCHING PATTERN "*.spv")
install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/assets_public")
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/CLI_CONTRACT_CN.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RC0_BASELINE_CN.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RC_RELEASE_CN.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender")
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/LICENSE.txt"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/licenses"
    RENAME "imgui-LICENSE.txt")
foreach(_pkg glfw3 tinygltf stb nlohmann-json)
    if(EXISTS
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${_pkg}/copyright")
        install(
            FILES
                "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/${_pkg}/copyright"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/licenses"
            RENAME "${_pkg}-LICENSE.txt")
    endif()
endforeach()
if(WIN32)
    install(FILES "$<TARGET_FILE:glfw>" DESTINATION "${CMAKE_INSTALL_BINDIR}")
elseif(UNIX)
    install(
        FILES "$<TARGET_FILE:glfw>"
        DESTINATION "${CMAKE_INSTALL_BINDIR}"
        RENAME "libglfw.so.3")
endif()

set(CPACK_PACKAGE_NAME "AzureRender")
set(CPACK_PACKAGE_VENDOR "AzureRender Project")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}-rc1")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vulkan stylized renderer and editor RC")
set(CPACK_PACKAGE_FILE_NAME
    "AzureRender-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)
