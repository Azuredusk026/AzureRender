# Optional module prototype. The installed production shaders remain the
# independently verified GLSL programs; module outputs have explicit names.
option(AZURE_ENABLE_SHADER_MODULE_PROTOTYPE "Build checked Slang shader module prototypes" OFF)
if(AZURE_ENABLE_SHADER_MODULE_PROTOTYPE)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    find_program(AZURE_SLANG_COMPILER slangc HINTS "$ENV{VULKAN_SDK}/Bin" REQUIRED)
    get_filename_component(AZURE_SLANG_DIRECTORY "${AZURE_SLANG_COMPILER}" DIRECTORY)
    file(GLOB AZURE_SLANG_LIBRARIES CONFIGURE_DEPENDS "${AZURE_SLANG_DIRECTORY}/slang*.dll")
    execute_process(COMMAND "${AZURE_SLANG_COMPILER}" -version
        OUTPUT_VARIABLE AZURE_SLANG_VERSION ERROR_VARIABLE AZURE_SLANG_VERSION_ERROR RESULT_VARIABLE AZURE_SLANG_VERSION_STATUS)
    string(STRIP "${AZURE_SLANG_VERSION}${AZURE_SLANG_VERSION_ERROR}" AZURE_SLANG_VERSION)
    if(NOT AZURE_SLANG_VERSION_STATUS EQUAL 0 OR NOT AZURE_SLANG_VERSION STREQUAL "2026.8")
        message(FATAL_ERROR "Shader module prototypes require Slang 2026.8")
    endif()
    set(AZURE_MODULE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shaders/modules")
    set(AZURE_MODULE_BINARY "${CMAKE_CURRENT_BINARY_DIR}/shader-modules")
    file(MAKE_DIRECTORY "${AZURE_MODULE_BINARY}")
    file(GLOB_RECURSE AZURE_MODULE_DEPENDENCIES CONFIGURE_DEPENDS "${AZURE_MODULE_SOURCE}/*.slang")
    add_custom_target(AzureShaderSharedTypes ALL
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_shader_types.py"
            --description "${AZURE_MODULE_SOURCE}/shared-layout.json"
            --cpp "${CMAKE_CURRENT_SOURCE_DIR}/src/render/ShaderSharedTypes.hpp"
            --slang "${AZURE_MODULE_SOURCE}/SharedTypes.slang" --check
        DEPENDS "${AZURE_MODULE_SOURCE}/shared-layout.json" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_shader_types.py"
        VERBATIM)
    foreach(AZURE_STRATEGY direct cached)
        if(AZURE_STRATEGY STREQUAL "cached")
            set(AZURE_CACHE_MODE 1)
        else()
            set(AZURE_CACHE_MODE 0)
        endif()
        set(AZURE_MODULE_OUTPUT "${AZURE_MODULE_BINARY}/bloom-${AZURE_STRATEGY}.spv")
        set(AZURE_MODULE_REQUEST "${AZURE_MODULE_BINARY}/${AZURE_STRATEGY}-request.json")
        file(GENERATE OUTPUT "${AZURE_MODULE_REQUEST}" CONTENT
            "{\"schemaVersion\":1,\"source\":\"${AZURE_MODULE_SOURCE}/Bloom.slang\",\"entry\":\"main\",\"target\":\"spirv\",\"profile\":\"spirv_1_3\",\"compiler\":\"${AZURE_SLANG_COMPILER}\",\"includeRoots\":[\"${AZURE_MODULE_SOURCE}\"],\"layout\":\"${AZURE_MODULE_SOURCE}/shared-layout.json\",\"defines\":{\"AZURE_BLOOM_CACHE\":\"${AZURE_CACHE_MODE}\"},\"output\":\"${AZURE_MODULE_OUTPUT}\"}\n")
        add_custom_command(OUTPUT "${AZURE_MODULE_OUTPUT}" "${AZURE_MODULE_OUTPUT}.json"
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_shader_module.py" --request "${AZURE_MODULE_REQUEST}"
            DEPENDS ${AZURE_MODULE_DEPENDENCIES} "${AZURE_MODULE_REQUEST}" "${AZURE_MODULE_SOURCE}/shared-layout.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_shader_module.py" "${AZURE_SLANG_COMPILER}" ${AZURE_SLANG_LIBRARIES}
            COMMENT "Compiling checked Bloom module (${AZURE_STRATEGY})" VERBATIM)
        list(APPEND AZURE_MODULE_OUTPUTS "${AZURE_MODULE_OUTPUT}" "${AZURE_MODULE_OUTPUT}.json")
    endforeach()
    add_custom_target(AzureShaderModulePrototypes ALL DEPENDS ${AZURE_MODULE_OUTPUTS})
    add_dependencies(AzureShaderModulePrototypes AzureShaderSharedTypes)
    install(FILES ${AZURE_MODULE_OUTPUTS} DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/shaders/modules")
    install(DIRECTORY "${AZURE_MODULE_SOURCE}/" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/shader-modules")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tools/compile_shader_module.py" "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_shader_types.py"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/tools")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/render/ShaderSharedTypes.hpp"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/include/render")
endif()
