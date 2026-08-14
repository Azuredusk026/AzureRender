cmake_minimum_required(VERSION 3.20)

project(
    AzureRender
    VERSION 0.1.0
    DESCRIPTION "Portfolio-first Vulkan stylized renderer"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(glfw3 CONFIG REQUIRED)
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
    src/editor/SceneModel.cpp
    src/editor/SceneModel.hpp
    src/platform/GlfwFrontend.cpp
    src/platform/GlfwFrontend.hpp
    src/render/RendererCore.cpp
    src/render/RendererCore.hpp
    src/render/RenderSettings.cpp
    src/render/RenderSettings.hpp
)

add_dependencies(AzureRender AzureRenderShaders)

target_link_libraries(
    AzureRender
    PRIVATE
        Vulkan::Vulkan
        glfw
)

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
        AZURERENDER_SHADER_DIR="${SHADER_BINARY_DIR}"
        AZURERENDER_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets_public"
        AZURERENDER_RAMP_PROFILE_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets_public/toon_ramp_profiles.json"
        AZURERENDER_RAMP_ATLAS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets_public/toon_ramp_atlas.ppm"
        AZURERENDER_CAPTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/captures"
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
