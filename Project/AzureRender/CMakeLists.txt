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
    src/platform/GlfwFrontend.cpp
    src/platform/GlfwFrontend.hpp
    src/render/RendererCore.cpp
    src/render/RendererCore.hpp
    src/render/RenderSettings.cpp
    src/render/RenderSettings.hpp
)

add_dependencies(AzureRender AzureRenderShaders)

if(BUILD_TESTING)
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
    add_test(
        NAME AzureRender.ExtensionRegistry
        COMMAND AzureRenderExtensionRegistryTests)
endif()

target_link_libraries(
    AzureRender
    PRIVATE
        Vulkan::Vulkan
        glfw
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
        AZURERENDER_VERSION="${PROJECT_VERSION}-rc0"
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
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RC0_BASELINE_CN.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/RC_RELEASE_CN.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender")
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
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}-rc0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vulkan stylized renderer and editor RC")
set(CPACK_PACKAGE_FILE_NAME
    "AzureRender-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)
