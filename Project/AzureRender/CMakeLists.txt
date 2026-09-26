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

# MSVC does not enable C++ unwind semantics by default. Without /EHsc every
# try/catch in the codebase and in the test executables is inert, so a thrown
# exception terminates the process instead of being caught. This must apply to
# all targets, not just the renderer, because the tests assert on error paths.
if(MSVC)
    add_compile_options(/EHsc /utf-8)
endif()

# Ninja tracks header dependencies on MSVC by matching the /showIncludes
# prefix recorded at compiler-detection time against the compiler's actual
# output bytes. On a localized toolchain the recorded prefix can diverge from
# reality (wrong code page during the probe), and then every header edit
# silently fails to trigger a rebuild; stale objects crash against new struct
# layouts at runtime. Probe the compiler here and compare byte-exactly so the
# misconfiguration is loud instead of corrupting objects.
if(MSVC AND CMAKE_GENERATOR MATCHES "Ninja")
    set(_azr_probe_dir
        "${CMAKE_BINARY_DIR}/CMakeFiles/AzureRenderShowIncludesProbe")
    file(WRITE "${_azr_probe_dir}/foo.h" "\n")
    file(WRITE "${_azr_probe_dir}/main.c" "#include \"foo.h\"\n")
    execute_process(
        COMMAND "${CMAKE_CXX_COMPILER}" /nologo /showIncludes /c main.c
        WORKING_DIRECTORY "${_azr_probe_dir}"
        OUTPUT_VARIABLE _azr_probe_out
        ERROR_VARIABLE _azr_probe_out
        RESULT_VARIABLE _azr_probe_result
        ENCODING NONE
    )
    string(FIND "${_azr_probe_out}" "foo.h" _azr_probe_has_include)
    string(FIND
        "${_azr_probe_out}"
        "${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}"
        _azr_prefix_pos)
    if(_azr_probe_result EQUAL 0 AND _azr_probe_has_include GREATER_EQUAL 0
       AND ("${CMAKE_CXX_CL_SHOWINCLUDES_PREFIX}" STREQUAL ""
            OR _azr_prefix_pos LESS 0))
        message(
            WARNING
            "Ninja header-dependency tracking is broken in this build tree: "
            "the recorded /showIncludes prefix does not match the compiler's "
            "output. Run 'python tools/fix_msvc_deps_prefix.py "
            "${CMAKE_BINARY_DIR}' and configure again, or expect stale "
            "objects after header edits.")
    endif()
    unset(_azr_probe_dir)
    unset(_azr_probe_out)
    unset(_azr_probe_result)
    unset(_azr_probe_has_include)
    unset(_azr_prefix_pos)
endif()

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)

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
    set(oneValueArgs OUTPUT_NAME)
    set(multiValueArgs DEFINES)
    cmake_parse_arguments(SHADER "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    if(SHADER_OUTPUT_NAME)
        set(shader_name "${SHADER_OUTPUT_NAME}")
    endif()
    set(shader_output "${SHADER_BINARY_DIR}/${shader_name}.spv")
    set(shader_define_args)
    foreach(shader_define IN LISTS SHADER_DEFINES)
        list(APPEND shader_define_args "-D${shader_define}")
    endforeach()

    add_custom_command(
        OUTPUT "${shader_output}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${SHADER_BINARY_DIR}"
        COMMAND Vulkan::glslc ${shader_define_args} "${source_file}" -o "${shader_output}"
        DEPENDS "${source_file}"
        COMMENT "Compiling ${shader_name}"
        VERBATIM
    )

    set(COMPILED_SHADERS ${COMPILED_SHADERS} "${shader_output}" PARENT_SCOPE)
endfunction()

compile_shader("${SHADER_SOURCE_DIR}/mesh.vert")
compile_shader("${SHADER_SOURCE_DIR}/mesh.frag")
compile_shader(
    "${SHADER_SOURCE_DIR}/mesh.frag"
    DEFINES AZURE_BINDLESS=1
    OUTPUT_NAME mesh_bindless.frag)
compile_shader("${SHADER_SOURCE_DIR}/outline.vert")
compile_shader("${SHADER_SOURCE_DIR}/outline.frag")
compile_shader("${SHADER_SOURCE_DIR}/background.vert")
compile_shader("${SHADER_SOURCE_DIR}/background.frag")
compile_shader(
    "${SHADER_SOURCE_DIR}/background.frag"
    DEFINES AZURE_BINDLESS=1
    OUTPUT_NAME background_bindless.frag)
compile_shader("${SHADER_SOURCE_DIR}/shadow.vert")
compile_shader("${SHADER_SOURCE_DIR}/shadow.frag")
compile_shader(
    "${SHADER_SOURCE_DIR}/shadow.frag"
    DEFINES AZURE_BINDLESS=1
    OUTPUT_NAME shadow_bindless.frag)
compile_shader("${SHADER_SOURCE_DIR}/inner_outline.vert")
compile_shader("${SHADER_SOURCE_DIR}/inner_outline.frag")
compile_shader("${SHADER_SOURCE_DIR}/hud.vert")
compile_shader("${SHADER_SOURCE_DIR}/hud.frag")
compile_shader("${SHADER_SOURCE_DIR}/blackhole.vert")
compile_shader("${SHADER_SOURCE_DIR}/blackhole.frag")
compile_shader("${SHADER_SOURCE_DIR}/blackhole_taa.frag")
compile_shader("${SHADER_SOURCE_DIR}/blackhole_composite.frag")
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
    src/render/EnvironmentAsset.cpp
    src/render/EnvironmentAsset.hpp
    src/rhi/GpuAllocator.cpp
    src/rhi/GpuAllocator.hpp
    src/rhi/IGpuAllocator.hpp
    src/rhi/NullRhi.cpp
    src/rhi/NullRhi.hpp
    src/rhi/Rhi.hpp
    src/rhi/RingFrameAllocator.hpp
    src/rhi/UploadRingBuffer.cpp
    src/rhi/UploadRingBuffer.hpp
    src/rhi/VulkanRhi.cpp
    src/rhi/VulkanRhi.hpp
    src/rhi/VulkanMemoryAllocatorImpl.cpp
    src/scenes/CharacterSceneRenderer.cpp
    src/scenes/CharacterSceneRenderer.hpp
    src/scenes/BlackholeSceneRenderer.cpp
    src/scenes/BlackholeSceneRenderer.hpp
    src/scenes/BuiltinRendererCatalog.cpp
    src/scenes/BuiltinRendererCatalog.hpp
    src/scenes/SampleSceneRenderer.cpp
    src/scenes/SampleSceneRenderer.hpp
)

add_dependencies(AzureRender AzureRenderShaders)

if(BUILD_TESTING)
    add_executable(AzureRenderRenderGraphTests tests/RenderGraphTests.cpp src/render/RenderGraph.cpp)
    target_include_directories(AzureRenderRenderGraphTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME AzureRender.RenderGraph COMMAND AzureRenderRenderGraphTests)
    add_executable(AzureRenderSurfaceLifecycleTests tests/SurfaceLifecycleTests.cpp)
    target_include_directories(AzureRenderSurfaceLifecycleTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME AzureRender.SurfaceLifecycle COMMAND AzureRenderSurfaceLifecycleTests)
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
    target_link_libraries(
        AzureRenderEditorSessionTests PRIVATE nlohmann_json::nlohmann_json)
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
    target_link_libraries(
        AzureRenderSceneModelTests PRIVATE nlohmann_json::nlohmann_json)
    target_compile_definitions(
        AzureRenderSceneModelTests PRIVATE
        AZURERENDER_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
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
    add_executable(
        AzureRenderUploadRingTests
        tests/UploadRingTests.cpp
    )
    target_include_directories(
        AzureRenderUploadRingTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(
        NAME AzureRender.UploadRing
        COMMAND AzureRenderUploadRingTests)
    add_executable(
        AzureRenderNullRhiTests
        tests/NullRhiTests.cpp
        src/rhi/NullRhi.cpp
    )
    target_include_directories(
        AzureRenderNullRhiTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(
        AzureRenderNullRhiTests PRIVATE Vulkan::Vulkan)
    add_test(
        NAME AzureRender.NullRhi
        COMMAND AzureRenderNullRhiTests)
    add_executable(
        AzureRenderNullRhiPassTests
        tests/NullRhiPassTests.cpp
        src/scenes/CharacterSceneRenderer.cpp
        src/assets/GltfLoader.cpp
        src/render/EnvironmentAsset.cpp
        src/render/RenderSettings.cpp
        src/diagnostics/RuntimeDiagnostics.cpp
        src/rhi/NullRhi.cpp
    )
    target_include_directories(
        AzureRenderNullRhiPassTests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
            "${TINYGLTF_INCLUDE_DIR}"
            "${STB_INCLUDE_DIR}")
    target_link_libraries(
        AzureRenderNullRhiPassTests PRIVATE
            Vulkan::Vulkan
            nlohmann_json::nlohmann_json)
    target_compile_definitions(
        AzureRenderNullRhiPassTests PRIVATE
        AZURERENDER_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(
        NAME AzureRender.NullRhiPass
        COMMAND AzureRenderNullRhiPassTests)
    add_executable(
        AzureRenderSceneGraphTests
        tests/SceneGraphTests.cpp
    )
    target_include_directories(
        AzureRenderSceneGraphTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(
        AzureRenderSceneGraphTests PRIVATE Vulkan::Vulkan)
    add_test(
        NAME AzureRender.SceneGraph
        COMMAND AzureRenderSceneGraphTests)

    # GPU visual regression. This needs a working Vulkan device, so it is
    # registered only when Python and the tooling are present and it reports
    # "skipped" rather than failing on machines without a usable GPU.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(
            NAME AzureRender.VisualRegression
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_visual_regression.py"
                --executable "$<TARGET_FILE:AzureRender>"
                --tolerance strict
                --summary
                    "${CMAKE_CURRENT_BINARY_DIR}/visual-regression-summary.json")
        set_tests_properties(
            AzureRender.VisualRegression
            PROPERTIES
                SKIP_RETURN_CODE 2
                TIMEOUT 900
                LABELS "gpu"
                RUN_SERIAL TRUE)
        # The fixed per-material descriptor path must stay pixel-identical;
        # it is the fallback on devices without descriptor indexing.
        add_test(
            NAME AzureRender.VisualRegressionLegacyDescriptors
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_visual_regression.py"
                --executable "$<TARGET_FILE:AzureRender>"
                --tolerance strict
                "--renderer-arg=--disable-bindless"
                --summary
                    "${CMAKE_CURRENT_BINARY_DIR}/visual-regression-legacy-summary.json")
        set_tests_properties(
            AzureRender.VisualRegressionLegacyDescriptors
            PROPERTIES
                SKIP_RETURN_CODE 2
                TIMEOUT 900
                LABELS "gpu"
                RUN_SERIAL TRUE)
    endif()
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
        GPUOpen::VulkanMemoryAllocator
)

# The VMA implementation unit is third-party code and does not satisfy the
# project warning gate; exempt only that file.
if(MSVC)
    set_source_files_properties(
        src/rhi/VulkanMemoryAllocatorImpl.cpp
        PROPERTIES COMPILE_OPTIONS "/W0")
else()
    set_source_files_properties(
        src/rhi/VulkanMemoryAllocatorImpl.cpp
        PROPERTIES COMPILE_OPTIONS "-w")
endif()

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
    # The strict warning gate is tuned for the GCC/MinGW baseline. MSVC reports
    # additional diagnostic classes that are not defects on this codebase:
    # C4456 (local shadowing of reused Vulkan *Info structs), C4996 (getenv),
    # and C4127 (conditions on compile-time constants such as kEnableValidation,
    # which is false in Release). They are demoted so the MSVC toolchain stays
    # buildable without weakening the GCC gate that CI enforces.
    target_compile_options(
        AzureRender
        PRIVATE
            /W4
            /WX
            /permissive-
            /wd4127
            /wd4456
            /wd4996
    )
else()
    target_compile_options(
        AzureRender
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Werror
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
    set(AZURERENDER_MINGW_RUNTIME_FILES "")
    foreach(runtime_dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${MINGW_RUNTIME_DIR}/${runtime_dll}")
            list(APPEND AZURERENDER_MINGW_RUNTIME_FILES
                "${MINGW_RUNTIME_DIR}/${runtime_dll}")
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
if(MINGW)
    if(NOT AZURERENDER_MINGW_RUNTIME_FILES)
        message(FATAL_ERROR "MinGW runtime DLLs were not found")
    endif()
    install(
        FILES ${AZURERENDER_MINGW_RUNTIME_FILES}
        DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
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
        "${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/index.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/getting-started.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/architecture.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/character-rendering.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/blackhole-rendering.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/assets-and-editor.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/development-and-release.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/reference.md"
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
