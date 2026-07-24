cmake_minimum_required(VERSION 3.20)

project(
    MyVulkanApp
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
add_custom_target(MyVulkanAppShaders ALL DEPENDS ${COMPILED_SHADERS})

add_executable(
    MyVulkanApp
    src/main.cpp
    src/app/VulkanApp.cpp
    src/app/VulkanApp.hpp
    src/assets/GltfLoader.cpp
    src/assets/GltfLoader.hpp
)

add_dependencies(MyVulkanApp MyVulkanAppShaders)

target_link_libraries(
    MyVulkanApp
    PRIVATE
        Vulkan::Vulkan
        glfw
)

target_include_directories(MyVulkanApp PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${TINYGLTF_INCLUDE_DIR}")

target_compile_definitions(
    MyVulkanApp
    PRIVATE
        GLFW_INCLUDE_VULKAN
        MYVULKANAPP_SHADER_DIR="${SHADER_BINARY_DIR}"
        MYVULKANAPP_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets_public"
        MYVULKANAPP_CAPTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/captures"
        $<$<CONFIG:Debug>:MYVULKANAPP_ENABLE_VALIDATION=1>
)

if(MSVC)
    target_compile_options(MyVulkanApp PRIVATE /W4 /permissive-)
else()
    target_compile_options(
        MyVulkanApp
        PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-missing-field-initializers
    )
endif()

if(WIN32)
    add_custom_command(
        TARGET MyVulkanApp
        POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:glfw>"
                "$<TARGET_FILE_DIR:MyVulkanApp>"
        COMMENT "Copying GLFW runtime"
    )
endif()

if(MINGW)
    get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(runtime_dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        if(EXISTS "${MINGW_RUNTIME_DIR}/${runtime_dll}")
            add_custom_command(
                TARGET MyVulkanApp
                POST_BUILD
                COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                        "${MINGW_RUNTIME_DIR}/${runtime_dll}"
                        "$<TARGET_FILE_DIR:MyVulkanApp>"
                COMMENT "Copying ${runtime_dll}"
            )
        endif()
    endforeach()
endif()

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT MyVulkanApp)
