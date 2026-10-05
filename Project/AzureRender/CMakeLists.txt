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

find_package(Jolt CONFIG REQUIRED)
find_package(sol2 CONFIG REQUIRED)
find_package(Lua 5.4 EXACT REQUIRED)
find_package(RmlUi CONFIG REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
find_package(Vulkan REQUIRED COMPONENTS glslc)
find_package(glfw3 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)
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
compile_shader("${SHADER_SOURCE_DIR}/cull_indirect.comp")
compile_shader(
    "${SHADER_SOURCE_DIR}/mesh.vert"
    DEFINES AZURE_COMPUTE_SKINNING=1
    OUTPUT_NAME mesh_compute.vert)
compile_shader("${SHADER_SOURCE_DIR}/mesh.frag")
compile_shader(
    "${SHADER_SOURCE_DIR}/mesh.frag"
    DEFINES AZURE_BINDLESS=1
    OUTPUT_NAME mesh_bindless.frag)
compile_shader("${SHADER_SOURCE_DIR}/outline.vert")
compile_shader(
    "${SHADER_SOURCE_DIR}/outline.vert"
    DEFINES AZURE_COMPUTE_SKINNING=1
    OUTPUT_NAME outline_compute.vert)
compile_shader("${SHADER_SOURCE_DIR}/outline.frag")
compile_shader("${SHADER_SOURCE_DIR}/background.vert")
compile_shader("${SHADER_SOURCE_DIR}/background.frag")
compile_shader(
    "${SHADER_SOURCE_DIR}/background.frag"
    DEFINES AZURE_BINDLESS=1
    OUTPUT_NAME background_bindless.frag)
compile_shader("${SHADER_SOURCE_DIR}/shadow.vert")
compile_shader(
    "${SHADER_SOURCE_DIR}/shadow.vert"
    DEFINES AZURE_COMPUTE_SKINNING=1
    OUTPUT_NAME shadow_compute.vert)
compile_shader("${SHADER_SOURCE_DIR}/shadow.frag")
compile_shader(
    "${SHADER_SOURCE_DIR}/shadow.frag"
    DEFINES AZURE_BINDLESS=1
    OUTPUT_NAME shadow_bindless.frag)
compile_shader("${SHADER_SOURCE_DIR}/inner_outline.vert")
compile_shader("${SHADER_SOURCE_DIR}/inner_outline.frag")
compile_shader("${SHADER_SOURCE_DIR}/hud.vert")
compile_shader("${SHADER_SOURCE_DIR}/hud.frag")
compile_shader("${SHADER_SOURCE_DIR}/game-ui.vert")
compile_shader("${SHADER_SOURCE_DIR}/game-ui.frag")
compile_shader("${SHADER_SOURCE_DIR}/blackhole.vert")
compile_shader("${SHADER_SOURCE_DIR}/blackhole.frag")
compile_shader("${SHADER_SOURCE_DIR}/blackhole_taa.frag")
compile_shader(
    "${SHADER_SOURCE_DIR}/blackhole_taa.frag"
    DEFINES AZURE_COMPUTE_BLOOM=1
    OUTPUT_NAME blackhole_taa_compute.frag)
compile_shader("${SHADER_SOURCE_DIR}/blackhole_composite.frag")
compile_shader("${SHADER_SOURCE_DIR}/clear.comp")
compile_shader("${SHADER_SOURCE_DIR}/bloom_downsample.comp")
compile_shader("${SHADER_SOURCE_DIR}/bloom_composite.comp")
compile_shader("${SHADER_SOURCE_DIR}/ibl_prefilter.comp")
compile_shader("${SHADER_SOURCE_DIR}/skin.comp")
add_custom_target(AzureRenderShaders ALL DEPENDS ${COMPILED_SHADERS})

# Engine targets expose explicit ownership and dependency boundaries.
add_library(AzureBuildOptions INTERFACE)
target_include_directories(AzureBuildOptions INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(AzureBuildOptions INTERFACE GLFW_INCLUDE_VULKAN
    AZURERENDER_VERSION="${PROJECT_VERSION}-rc1"
    $<$<CONFIG:Debug>:AZURERENDER_ENABLE_VALIDATION=1>)
if(MSVC)
    target_compile_options(AzureBuildOptions INTERFACE /W4 /WX /permissive- /wd4127 /wd4456 /wd4996)
else()
    target_compile_options(AzureBuildOptions INTERFACE -Wall -Wextra -Wpedantic -Werror -Wno-missing-field-initializers)
endif()
add_library(AzureFoundation STATIC
    src/diagnostics/RuntimeDiagnostics.cpp
    src/resources/ResourceLocator.cpp
)
add_library(AzurePlatform STATIC
    src/platform/GlfwFrontend.cpp
)
add_library(AzureRenderCore STATIC
    src/assets/GltfLoader.cpp
    src/diagnostics/GpuCapabilityReport.cpp
    src/render/RendererCore.cpp
    src/render/GameUiRenderer.cpp
    src/render/RenderGraph.cpp
    src/render/TransientResourcePool.cpp
    src/render/RenderSettings.cpp
    src/render/EnvironmentAsset.cpp
    src/render/ComputePass.cpp
    src/rhi/GpuAllocator.cpp
    src/rhi/NullRhi.cpp
    src/rhi/UploadRingBuffer.cpp
    src/rhi/VulkanRhi.cpp
    src/rhi/VulkanMemoryAllocatorImpl.cpp
    src/scenes/CharacterSceneRenderer.cpp
    src/scenes/BlackholeSceneRenderer.cpp
    src/scenes/BuiltinRendererCatalog.cpp
    src/scenes/SampleSceneRenderer.cpp
)
add_executable(AzureMetaGen tools/metagen/main.cpp)
set(AZURE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AZURE_REFLECTION_OUTPUT "${AZURE_GENERATED_DIR}/reflection/GeneratedRegistry.hpp")
add_custom_command(OUTPUT "${AZURE_REFLECTION_OUTPUT}"
    COMMAND AzureMetaGen "${AZURE_REFLECTION_OUTPUT}" ecs/Components.hpp runtime/GameComponents.hpp
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src"
    DEPENDS AzureMetaGen src/ecs/Components.hpp src/runtime/GameComponents.hpp
    VERBATIM)
add_library(AzureReflection STATIC src/reflection/Registry.cpp "${AZURE_REFLECTION_OUTPUT}")
target_include_directories(AzureReflection PUBLIC "${AZURE_GENERATED_DIR}")
target_link_libraries(AzureReflection PUBLIC AzureFoundation nlohmann_json::nlohmann_json)
add_library(AzureRuntime STATIC
    src/app/CommandLine.cpp
    src/runtime/SceneDocument.cpp
    src/runtime/SceneRenderSnapshot.cpp
    src/runtime/Project.cpp
    src/runtime/AssetDatabase.cpp
    src/runtime/Level.cpp
    src/runtime/Prefab.cpp
    src/runtime/LevelSession.cpp
    src/runtime/PhysicsWorld.cpp
    src/runtime/GameRuntime.cpp
    src/runtime/ScriptRuntime.cpp
    src/runtime/GameUi.cpp
    src/runtime/AnimationStateMachine.cpp
    src/runtime/PresentationRuntime.cpp
    src/runtime/AudioRuntime.cpp
    src/runtime/MiniaudioImpl.cpp
)
add_library(AzureEditor STATIC
    src/editor/EditorCameraController.cpp
    src/editor/EditorContext.cpp
    src/editor/EditorProjectEditing.cpp
    src/editor/AssetImportJob.cpp
    src/editor/EditorSession.cpp
    src/editor/GameBuildJob.cpp
    src/editor/EditorAutomation.cpp
    src/editor/ImGuiEditorLayer.cpp
)
set(AZURE_HOST_SOURCES
    src/app/AzureRenderApp.cpp
    src/app/AzureRenderGameplay.cpp
    src/app/AzureRenderCapture.cpp
    src/app/AzureRenderDescriptors.cpp
    src/app/AzureRenderFrame.cpp
    src/app/AzureRenderPipeline.cpp
    src/app/AzureRenderResources.cpp
    src/app/AzureRenderSupport.cpp
)

target_link_libraries(AzureFoundation PUBLIC AzureBuildOptions)
target_compile_definitions(AzureFoundation PRIVATE AZURERENDER_DEVELOPMENT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(AzurePlatform PUBLIC AzureFoundation Vulkan::Vulkan glfw)
target_link_libraries(AzureRenderCore PUBLIC AzureFoundation Vulkan::Vulkan nlohmann_json::nlohmann_json RmlUi::RmlUi
    GPUOpen::VulkanMemoryAllocator OpenEXR::OpenEXR)
target_include_directories(AzureRenderCore PRIVATE "${TINYGLTF_INCLUDE_DIR}" "${STB_INCLUDE_DIR}")
target_link_libraries(AzureRuntime PUBLIC AzureRenderCore AzureReflection Jolt::Jolt RmlUi::RmlUi PRIVATE sol2::sol2 ${LUA_LIBRARIES})
target_include_directories(AzureRuntime PRIVATE ${LUA_INCLUDE_DIR})
target_include_directories(AzureRuntime SYSTEM PRIVATE ${MINIAUDIO_INCLUDE_DIR})
if(WIN32)
    target_link_libraries(AzureRuntime PRIVATE winmm ole32 uuid)
endif()
target_link_libraries(AzureEditor PUBLIC AzureRuntime AzurePlatform)
if(TARGET imgui::imgui)
    target_link_libraries(AzureEditor PUBLIC imgui::imgui)
    target_compile_definitions(AzureEditor PRIVATE AZURERENDER_HAS_IMGUI=1)
endif()
add_library(AzureRenderHost STATIC ${AZURE_HOST_SOURCES})
target_link_libraries(AzureRenderHost PUBLIC AzureRuntime AzurePlatform AzureEditor)
target_compile_definitions(AzureRenderHost PUBLIC AZURE_WITH_EDITOR=1)
if(TARGET imgui::imgui)
    target_compile_definitions(AzureRenderHost PRIVATE AZURERENDER_HAS_IMGUI=1)
endif()
add_library(AzurePlayerHost STATIC ${AZURE_HOST_SOURCES})
target_link_libraries(AzurePlayerHost PUBLIC AzureRuntime AzurePlatform)
target_compile_definitions(AzurePlayerHost PUBLIC AZURE_WITH_EDITOR=0)
foreach(AZURE_HOST_TARGET AzureRenderHost AzurePlayerHost)
    target_include_directories(${AZURE_HOST_TARGET} PRIVATE "${TINYGLTF_INCLUDE_DIR}" "${STB_INCLUDE_DIR}")
endforeach()
add_executable(AzureRender src/main.cpp)
target_link_libraries(AzureRender PRIVATE AzureRenderHost)
add_executable(AzurePlayer src/player/main.cpp)
target_link_libraries(AzurePlayer PRIVATE AzurePlayerHost)
add_dependencies(AzureRender AzureRenderShaders)
add_dependencies(AzurePlayer AzureRenderShaders)

if(BUILD_TESTING)
    add_executable(AzureGameUiRendererTests tests/GameUiRendererTests.cpp)
    target_link_libraries(AzureGameUiRendererTests PRIVATE AzureRenderCore RmlUi::RmlUi)
    add_dependencies(AzureGameUiRendererTests AzureRenderShaders)
    add_test(NAME AzureEngine.GameUiRenderer COMMAND AzureGameUiRendererTests "${SHADER_BINARY_DIR}")
    add_executable(AzureGameUiTests tests/GameUiTests.cpp)
    target_link_libraries(AzureGameUiTests PRIVATE AzureRuntime RmlUi::RmlUi)
    add_test(NAME AzureEngine.GameUi COMMAND AzureGameUiTests "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/gameplay/assets/hud.rml" "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/fonts/LatoLatin-Regular.ttf")
    add_executable(AzureEditorWorkflowTests tests/EditorWorkflowTests.cpp)
    target_link_libraries(AzureEditorWorkflowTests PRIVATE AzureEditor)
    add_test(NAME AzureEngine.EditorWorkflow COMMAND AzureEditorWorkflowTests "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/gameplay")
    add_executable(AzurePresentationRuntimeTests tests/PresentationRuntimeTests.cpp)
    target_link_libraries(AzurePresentationRuntimeTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.PresentationRuntime COMMAND AzurePresentationRuntimeTests "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/gameplay/project.azureproject")
    add_executable(AzureGameplayPresentationTests tests/GameplayPresentationTests.cpp)
    target_link_libraries(AzureGameplayPresentationTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.GameplayPresentation COMMAND AzureGameplayPresentationTests "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/gameplay/assets/portal.wav")
    add_executable(AzureScriptRuntimeTests tests/ScriptRuntimeTests.cpp)
    target_link_libraries(AzureScriptRuntimeTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.ScriptRuntime COMMAND AzureScriptRuntimeTests)
    add_executable(AzurePhysicsInputTests tests/PhysicsInputTests.cpp)
    add_executable(AzureThirdPersonTests tests/ThirdPersonTests.cpp)
    target_link_libraries(AzureThirdPersonTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.ThirdPerson COMMAND AzureThirdPersonTests)
    add_executable(AzureLocomotionTests tests/LocomotionTests.cpp)
    target_link_libraries(AzureLocomotionTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.Locomotion COMMAND AzureLocomotionTests)
    add_executable(AzureInteractionTests tests/InteractionTests.cpp)
    target_link_libraries(AzureInteractionTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.Interaction COMMAND AzureInteractionTests)
    add_executable(AzureScriptEntityTests tests/ScriptEntityTests.cpp)
    target_link_libraries(AzureScriptEntityTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.ScriptEntities COMMAND AzureScriptEntityTests)
    add_executable(AzurePlayableQuestTests tests/PlayableQuestTests.cpp)
    target_link_libraries(AzurePlayableQuestTests PRIVATE AzureRuntime)
    target_compile_definitions(AzurePlayableQuestTests PRIVATE AZURE_QUEST_PROJECT="${CMAKE_CURRENT_SOURCE_DIR}/assets_public/exploration")
    add_test(NAME AzureEngine.PlayableQuest COMMAND AzurePlayableQuestTests)
    add_executable(AzureBrowRegionTests tests/BrowRegionTests.cpp)
    add_executable(AzureAnimationPoseTests tests/AnimationPoseTests.cpp)
    target_include_directories(AzureAnimationPoseTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AzureAnimationPoseTests PRIVATE AzureRenderCore)
    add_test(NAME AzureRender.AnimationPose COMMAND AzureAnimationPoseTests)
    target_include_directories(AzureBrowRegionTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AzureBrowRegionTests PRIVATE AzureRenderCore)
    add_test(NAME AzureRender.BrowRegions COMMAND AzureBrowRegionTests)
    target_link_libraries(AzurePhysicsInputTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.PhysicsInput COMMAND AzurePhysicsInputTests)
    add_executable(AzureAssetLevelTests tests/AssetLevelTests.cpp)
    target_link_libraries(AzureAssetLevelTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.AssetLevel COMMAND AzureAssetLevelTests)
    add_executable(AzureReflectionTests tests/ReflectionTests.cpp)
    target_link_libraries(AzureReflectionTests PRIVATE AzureReflection)
    add_test(NAME AzureEngine.Reflection COMMAND AzureReflectionTests)
    add_executable(AzureSceneVisibilityTests tests/SceneVisibilityTests.cpp)
    target_link_libraries(AzureSceneVisibilityTests PRIVATE AzureRuntime)
    add_test(NAME AzureRender.SceneVisibility COMMAND AzureSceneVisibilityTests
        "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/exploration/assets/cube.gltf")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_test(NAME AzureEngine.RepositoryAudit COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_repository_audit.py")
    add_test(NAME AzureRender.SceneVisibilityGpu COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_scene_visibility_gpu.py" --executable "$<TARGET_FILE:AzurePlayer>")
    set_tests_properties(AzureRender.SceneVisibilityGpu PROPERTIES LABELS gpu RUN_SERIAL TRUE TIMEOUT 300)
    add_test(NAME AzureEngine.PublicGait COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_public_gait.py")
    add_test(NAME AzureEngine.LocomotionPlayer COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_locomotion_direction.py" --player "$<TARGET_FILE:AzurePlayer>")
    set_tests_properties(AzureEngine.LocomotionPlayer PROPERTIES LABELS gpu RUN_SERIAL TRUE TIMEOUT 300)
    add_test(NAME AzureEngine.AnimationRetarget COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_animation_retarget.py")
    add_test(NAME AzureEngine.SourceSkinRestore COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_skin_restore.py")
    add_test(NAME AzureEngine.GamePackageRules COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_game_package_rules.py")
    add_test(NAME AzureEngine.PlayableDeliveryRules COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_playable_delivery_rules.py")
    include("${CMAKE_CURRENT_SOURCE_DIR}/tools/register_game_package.cmake")
    azure_register_game_package()
    add_test(NAME AzureEngine.BuildConfiguration COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_build_configuration.py")
    add_test(NAME AzureEngine.BuildProvenance COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_build_provenance.py")
    add_test(NAME AzureEngine.CharacterIntake COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_character_intake.py")
    add_test(NAME AzureEngine.CharacterPreparation COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_character_preparation.py")
    add_test(NAME AzureEngine.MaterialFixtureGpu COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_material_fixture.py" --executable $<TARGET_FILE:AzureRender> --asset "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/third_person/material_fixture.gltf")
    add_test(NAME AzureEngine.IndependentAnimationGpu COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_independent_animation_gpu.py" --executable $<TARGET_FILE:AzurePlayer>)
    add_test(NAME AzureEngine.ThirdPersonPlayer COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_third_person_player.py" --executable $<TARGET_FILE:AzurePlayer>)
    set_tests_properties(AzureEngine.IndependentAnimationGpu AzureEngine.ThirdPersonPlayer PROPERTIES TIMEOUT 300 LABELS "gpu" RUN_SERIAL TRUE)
    add_test(NAME AzureEngine.PlayablePlayer COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_playable_player.py" --executable $<TARGET_FILE:AzurePlayer>)
    set_tests_properties(AzureEngine.PlayablePlayer PROPERTIES TIMEOUT 600 LABELS "gpu" RUN_SERIAL TRUE)
    add_test(NAME AzureEngine.RuntimeSpawnGpu COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_runtime_spawn_gpu.py" --executable $<TARGET_FILE:AzurePlayer>)
    set_tests_properties(AzureEngine.RuntimeSpawnGpu PROPERTIES TIMEOUT 300 LABELS "gpu" RUN_SERIAL TRUE)
    set_tests_properties(AzureEngine.MaterialFixtureGpu PROPERTIES LABELS gpu RUN_SERIAL TRUE TIMEOUT 240)
    add_test(NAME AzureEngine.PresentationGpu COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_game_presentation.py" --executable $<TARGET_FILE:AzurePlayer> --project "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/gameplay/project.azureproject")
    set_tests_properties(AzureEngine.PresentationGpu PROPERTIES LABELS gpu RUN_SERIAL TRUE TIMEOUT 180)
    add_test(NAME AzureEngine.EditorHost COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_editor_workflow.py" --executable $<TARGET_FILE:AzureRender> --project "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/gameplay/project.azureproject")
    add_test(NAME AzureEngine.EditorPlayable COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_editor_playable.py" --executable $<TARGET_FILE:AzureRender>)
    set_tests_properties(AzureEngine.EditorPlayable PROPERTIES LABELS "gpu" TIMEOUT 240)
    set_tests_properties(AzureEngine.EditorHost PROPERTIES LABELS gpu RUN_SERIAL TRUE TIMEOUT 120)
    add_test(NAME AzureEngine.MetaGenerator COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_metagen.py" $<TARGET_FILE:AzureMetaGen>)
    add_test(NAME AzureEngine.LevelPlayer COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_level_player.py" $<TARGET_FILE:AzurePlayer>)
    set_tests_properties(AzureEngine.LevelPlayer PROPERTIES LABELS gpu)
    add_test(NAME AzureEngine.PhysicsPlayer COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_physics_player.py" $<TARGET_FILE:AzurePlayer>)
    add_test(NAME AzureEngine.ScriptPlayer COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_script_player.py" $<TARGET_FILE:AzurePlayer> "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/gameplay")
    set_tests_properties(AzureEngine.ScriptPlayer PROPERTIES LABELS gpu)
    set_tests_properties(AzureEngine.PhysicsPlayer PROPERTIES LABELS gpu)
    add_executable(AzureProjectTests tests/ProjectTests.cpp)
    target_link_libraries(AzureProjectTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.Project COMMAND AzureProjectTests)
    add_executable(AzureRuntimeLifecycleTests tests/RuntimeLifecycleTests.cpp)
    target_link_libraries(AzureRuntimeLifecycleTests PRIVATE AzureRuntime)
    add_test(NAME AzureEngine.RuntimeLifecycle COMMAND AzureRuntimeLifecycleTests)
    add_executable(AzureRenderEnvironmentAssetTests tests/EnvironmentAssetTests.cpp src/render/EnvironmentAsset.cpp)
    target_include_directories(AzureRenderEnvironmentAssetTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src" "${STB_INCLUDE_DIR}")
    target_link_libraries(AzureRenderEnvironmentAssetTests PRIVATE OpenEXR::OpenEXR)
    add_test(NAME AzureRender.EnvironmentAsset COMMAND AzureRenderEnvironmentAssetTests)
    add_executable(AzureRenderRenderGraphTests tests/RenderGraphTests.cpp src/render/RenderGraph.cpp src/rhi/NullRhi.cpp)
    target_include_directories(AzureRenderRenderGraphTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AzureRenderRenderGraphTests PRIVATE Vulkan::Vulkan)
    add_test(NAME AzureRender.RenderGraph COMMAND AzureRenderRenderGraphTests)
    add_executable(AzureRenderTransientResourcePoolTests tests/TransientResourcePoolTests.cpp src/render/TransientResourcePool.cpp src/rhi/NullRhi.cpp)
    target_include_directories(AzureRenderTransientResourcePoolTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AzureRenderTransientResourcePoolTests PRIVATE Vulkan::Vulkan)
    add_test(NAME AzureRender.TransientResourcePool COMMAND AzureRenderTransientResourcePoolTests)
    add_executable(AzureRenderLightBufferTests tests/LightBufferTests.cpp)
    target_include_directories(AzureRenderLightBufferTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME AzureRender.LightBuffer COMMAND AzureRenderLightBufferTests)
    add_executable(AzureRenderCascadedShadowTests tests/CascadedShadowTests.cpp)
    target_include_directories(AzureRenderCascadedShadowTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME AzureRender.CascadedShadow COMMAND AzureRenderCascadedShadowTests)
    add_executable(AzureRenderClusteredLightGridTests tests/ClusteredLightGridTests.cpp)
    target_include_directories(AzureRenderClusteredLightGridTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME AzureRender.ClusteredLightGrid COMMAND AzureRenderClusteredLightGridTests)
    add_executable(
        AzureRenderComputePassTests
        tests/ComputePassTests.cpp
        src/render/ComputePass.cpp
        src/render/ComputePass.hpp
        src/rhi/NullRhi.cpp
        src/rhi/NullRhi.hpp)
    target_include_directories(AzureRenderComputePassTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AzureRenderComputePassTests PRIVATE Vulkan::Vulkan)
    add_test(NAME AzureRender.ComputePass COMMAND AzureRenderComputePassTests)
    add_executable(AzureRenderFrameTaskSchedulerTests tests/FrameTaskSchedulerTests.cpp)
    target_include_directories(AzureRenderFrameTaskSchedulerTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(AzureRenderFrameTaskSchedulerTests PRIVATE Vulkan::Vulkan)
    add_test(NAME AzureRender.FrameTaskScheduler COMMAND AzureRenderFrameTaskSchedulerTests)
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
    add_executable(AzureRenderEditorSessionTests tests/EditorSessionTests.cpp)
    target_link_libraries(AzureRenderEditorSessionTests PRIVATE AzureEditor)
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
        src/runtime/SceneDocument.cpp
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
        src/render/RenderGraph.cpp
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
        src/scenes/BlackholeSceneRenderer.cpp
        src/render/ComputePass.cpp
        src/render/RenderGraph.cpp
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
            nlohmann_json::nlohmann_json
            OpenEXR::OpenEXR)
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
                --output-dir "${CMAKE_CURRENT_BINARY_DIR}/visual-regression"
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
                --output-dir "${CMAKE_CURRENT_BINARY_DIR}/visual-regression-legacy"
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
    if(WIN32)
        add_test(
            NAME AzureRender.BlackholeComputeGpu
            COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_blackhole_compute_smoke.ps1"
                -Executable "$<TARGET_FILE:AzureRender>"
                -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/blackhole-compute-smoke")
        set_tests_properties(
            AzureRender.BlackholeComputeGpu
            PROPERTIES
                SKIP_RETURN_CODE 2
                TIMEOUT 180
                LABELS "gpu"
                RUN_SERIAL TRUE)
        add_test(
            NAME AzureRender.CharacterIblComputeGpu
            COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_character_ibl_smoke.ps1"
                -Executable "$<TARGET_FILE:AzureRender>"
                -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/character-ibl-smoke")
        set_tests_properties(
            AzureRender.CharacterIblComputeGpu
            PROPERTIES
                SKIP_RETURN_CODE 2
                TIMEOUT 180
                LABELS "gpu"
                RUN_SERIAL TRUE)
        add_test(
            NAME AzureRender.CharacterSkinningComputeGpu
            COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_character_skinning_smoke.ps1"
                -Executable "$<TARGET_FILE:AzureRender>"
                -OutputDirectory "${CMAKE_CURRENT_BINARY_DIR}/character-skinning-smoke")
        set_tests_properties(
            AzureRender.CharacterSkinningComputeGpu
            PROPERTIES
                SKIP_RETURN_CODE 2
                TIMEOUT 180
                LABELS "gpu"
                RUN_SERIAL TRUE)
    endif()
    if(WIN32 AND Python3_Interpreter_FOUND)
        add_test(NAME AzureRender.R4AcceptanceRules
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_r4_acceptance.py")
        add_test(NAME AzureEngine.PlayerCli
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_player_cli.py"
                --executable "$<TARGET_FILE:AzurePlayer>")
        add_test(NAME AzureRender.BlackholeOptimizationRules
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_blackhole_optimization.py")
        add_test(NAME AzureRender.R3Comparison
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_r3_comparison.py"
                --executable "$<TARGET_FILE:AzureRender>"
                --output-dir "${CMAKE_CURRENT_BINARY_DIR}/r3-comparison"
                --frames 10 --instances 64)
        set_tests_properties(AzureRender.R3Comparison PROPERTIES
            TIMEOUT 900 LABELS "gpu" RUN_SERIAL TRUE)
        add_test(NAME AzureRender.R3TransparentComparison
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_r3_comparison.py"
                --executable "$<TARGET_FILE:AzureRender>"
                --output-dir "${CMAKE_CURRENT_BINARY_DIR}/r3-transparent-comparison"
                --frames 10 --instances 16 --transparent-fixture)
        set_tests_properties(AzureRender.R3TransparentComparison PROPERTIES
            TIMEOUT 900 LABELS "gpu" RUN_SERIAL TRUE)
        add_test(NAME AzureRender.AuditRegressions
            COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_audit_regressions.py"
                --executable "$<TARGET_FILE:AzureRender>")
        set_tests_properties(AzureRender.AuditRegressions PROPERTIES
            TIMEOUT 90 LABELS "gpu" RUN_SERIAL TRUE)
        add_test(
            NAME AzureRender.VisualRegressionLegacySkinning
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_visual_regression.py"
                --executable "$<TARGET_FILE:AzureRender>"
                --tolerance strict
                "--renderer-arg=--disable-compute-skinning"
                --output-dir "${CMAKE_CURRENT_BINARY_DIR}/visual-regression-legacy-skinning"
                --summary
                    "${CMAKE_CURRENT_BINARY_DIR}/visual-regression-legacy-skinning-summary.json")
        set_tests_properties(
            AzureRender.VisualRegressionLegacySkinning
            PROPERTIES
                SKIP_RETURN_CODE 2
                TIMEOUT 900
                LABELS "gpu"
                RUN_SERIAL TRUE)
        add_test(
            NAME AzureRender.CharacterMorphComputeGpu
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_morph_compute_regression.py"
                --executable "$<TARGET_FILE:AzureRender>"
                --output-dir
                    "${CMAKE_CURRENT_BINARY_DIR}/morph-compute-regression")
        set_tests_properties(
            AzureRender.CharacterMorphComputeGpu
            PROPERTIES
                SKIP_RETURN_CODE 2
                TIMEOUT 180
                LABELS "gpu"
                RUN_SERIAL TRUE)
        add_test(
            NAME AzureRender.ClusteredLightingGpu
            COMMAND
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/run_clustered_lighting_smoke.py"
                --executable "$<TARGET_FILE:AzureRender>"
                --output-dir
                    "${CMAKE_CURRENT_BINARY_DIR}/clustered-lighting-smoke")
        set_tests_properties(
            AzureRender.ClusteredLightingGpu
            PROPERTIES
                SKIP_RETURN_CODE 2
                TIMEOUT 180
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
        OpenEXR::OpenEXR
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
install(TARGETS AzureRender AzurePlayer AzureMetaGen RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
if(MSVC)
    if(MSVC_VERSION GREATER_EQUAL 1930)
        # CMake 3.20's runtime module recognizes VC142; VS2022 ships VC143.
        set(AZURE_MSVC_REDIST_ROOT "$ENV{VCToolsRedistDir}" CACHE PATH "MSVC redistributable root")
        get_filename_component(AZURE_MSVC_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
        get_filename_component(AZURE_MSVC_VC "${AZURE_MSVC_BIN}/../../../../../.." ABSOLUTE)
        file(GLOB AZURE_MSVC_CRT_CANDIDATES
            "${AZURE_MSVC_REDIST_ROOT}/x64/Microsoft.VC143.CRT/msvcp140.dll"
            "${AZURE_MSVC_VC}/Redist/MSVC/*/x64/Microsoft.VC143.CRT/msvcp140.dll")
        list(SORT AZURE_MSVC_CRT_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
        if(NOT AZURE_MSVC_CRT_CANDIDATES)
            message(FATAL_ERROR "VC143 runtime missing; set AZURE_MSVC_REDIST_ROOT to its redistributable root")
        endif()
        list(GET AZURE_MSVC_CRT_CANDIDATES 0 AZURE_MSVC_CRT_FILE)
        get_filename_component(AZURE_MSVC_CRT_DIR "${AZURE_MSVC_CRT_FILE}" DIRECTORY)
        file(GLOB AZURE_MSVC_CRT_DLLS "${AZURE_MSVC_CRT_DIR}/*.dll")
        install(FILES ${AZURE_MSVC_CRT_DLLS} DESTINATION "${CMAKE_INSTALL_BINDIR}")
    else()
        set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION "${CMAKE_INSTALL_BINDIR}")
        include(InstallRequiredSystemLibraries)
    endif()
endif()
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
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/assets_public"
    PATTERN ".azure" EXCLUDE
    PATTERN "*.tmp" EXCLUDE)
set(AZURE_DOCUMENTATION_FILES
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
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/documentation-standard.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/azure-engine-plan.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/render-core-implementation.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/compute-passes.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/character-finish.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/environment-assets.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/clustered-lighting.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/render-graph.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/rhi-synchronization.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/scene-data.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/parallel-submission.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/windows-acceptance.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/engine-foundation.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/g0-implementation.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/g1-implementation.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/g2-implementation.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/g3-implementation.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/g4-implementation.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/reflection.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/assets-levels.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/physics-input.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/scripts-gameplay.md")
install(FILES ${AZURE_DOCUMENTATION_FILES}
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender")
foreach(AZURE_DOCUMENTATION_FILE IN LISTS AZURE_DOCUMENTATION_FILES)
    file(RELATIVE_PATH AZURE_DOCUMENTATION_RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "${AZURE_DOCUMENTATION_FILE}")
    if(AZURE_DOCUMENTATION_RELATIVE MATCHES "^docs/")
        get_filename_component(AZURE_DOCUMENTATION_DIRECTORY "${AZURE_DOCUMENTATION_RELATIVE}" DIRECTORY)
        install(FILES "${AZURE_DOCUMENTATION_FILE}"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/${AZURE_DOCUMENTATION_DIRECTORY}")
    endif()
endforeach()
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/r1/2026-09-26.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/r1")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/r2/2026-09-26.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/r2")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/r3/2026-10-03.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/r3")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/r4/2026-10-03.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/r4")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/blackhole-performance.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/runtime")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/2026-10-03-blackhole-optimization.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/plans")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/g0/2026-10-03.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/g0")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/blackhole/2026-10-03.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/blackhole")
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/LICENSE.txt"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/licenses"
    RENAME "imgui-LICENSE.txt")
foreach(_pkg glfw3 tinygltf stb openexr imath libdeflate openjph nlohmann-json joltphysics lua sol2 rmlui miniaudio freetype libpng zlib bzip2 brotli)
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
    file(GLOB AZURE_RUNTIME_DLLS
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/*.dll")
    install(FILES ${AZURE_RUNTIME_DLLS}
        DESTINATION "${CMAKE_INSTALL_BINDIR}")
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
if(WIN32)
    if(CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]64$"
        OR CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^[Aa][Rr][Mm]64$")
        set(AZURERENDER_PACKAGE_ARCH "arm64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(AZURERENDER_PACKAGE_ARCH "x64")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(AZURERENDER_PACKAGE_ARCH "x86")
    else()
        message(FATAL_ERROR
            "Cannot determine Windows package architecture from the configured target")
    endif()
else()
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" AZURERENDER_PACKAGE_ARCH)
    if(AZURERENDER_PACKAGE_ARCH STREQUAL "")
        message(FATAL_ERROR "CMAKE_SYSTEM_PROCESSOR is required for package naming")
    endif()
endif()
set(CPACK_PACKAGE_FILE_NAME
    "AzureRender-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${AZURERENDER_PACKAGE_ARCH}")
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/runtime-build-$<CONFIG>.json"
    CONTENT "{\n  \"schemaVersion\": 1,\n  \"engineVersion\": \"${PROJECT_VERSION}\",\n  \"configuration\": \"$<CONFIG>\",\n  \"architecture\": \"${AZURERENDER_PACKAGE_ARCH}\",\n  \"platform\": \"${CMAKE_SYSTEM_NAME}\"\n}\n")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/runtime-build-$<CONFIG>.json"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender" RENAME "runtime-build.json")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_game.py"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/tools")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)

foreach(AZURE_PHASE f1 r5 g5 g6 u1 f2 p1 f3 r6 g7 g1 g2 g3 g4 u0 p0)
    file(GLOB AZURE_PHASE_RECORDS "${CMAKE_CURRENT_SOURCE_DIR}/docs/acceptance/${AZURE_PHASE}/*.md")
    if(AZURE_PHASE_RECORDS)
        install(FILES ${AZURE_PHASE_RECORDS} DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/acceptance/${AZURE_PHASE}")
    endif()
endforeach()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets_public/fonts/RmlUi-Sample-Fonts-LICENSE.txt" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/licenses" RENAME "LatoLatin-LICENSE.txt")

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/editor-game-ui.md" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/runtime")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/game-publishing.md" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/runtime")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/build-reproducibility.md" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/runtime")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/exploration-gameplay.md" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/runtime")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/playable-performance.md" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/runtime")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/runtime/scene-visibility.md" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/runtime")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/u0-implementation.md" "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/p0-implementation.md" "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/third-person-playable-plan.md" DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/plans")
install(FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/2026-10-05-quality-round.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/2026-10-05-repository-plan.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/2026-10-05-playable-quality-plan.md"
    "${CMAKE_CURRENT_SOURCE_DIR}/docs/plans/2026-10-05-editor-ui-plan.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/AzureRender/docs/plans")
