# Round-trip test for the AR-5.5 install manifest tooling.
#
# Builds a small synthetic install tree, writes a manifest, verifies it, then
# corrupts one file and expects verification to fail. Succeeds only when both
# the positive and negative checks behave correctly.

cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

set(WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/install-manifest-test")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}/bin")
file(MAKE_DIRECTORY "${WORK_DIR}/share/AzureRender/licenses")

# The manifest document itself lives outside the tree so it never becomes a
# scanned entry.
set(MANIFEST_FILE "${CMAKE_CURRENT_BINARY_DIR}/install-manifest-test-manifest.json")

file(WRITE "${WORK_DIR}/bin/AzureRender.exe" "MZ fake binary payload")
file(WRITE "${WORK_DIR}/share/AzureRender/README.md" "# AzureRender\n")
file(WRITE
    "${WORK_DIR}/share/AzureRender/licenses/imgui-LICENSE.txt"
    "MIT License\nCopyright (c) Dear ImGui\n")
file(WRITE
    "${WORK_DIR}/share/AzureRender/licenses/glfw3-LICENSE.txt"
    "zlib license\n")
file(WRITE
    "${WORK_DIR}/share/AzureRender/licenses/tinygltf-LICENSE.txt"
    "MIT License\n")
file(WRITE
    "${WORK_DIR}/share/AzureRender/licenses/stb-LICENSE.txt"
    "MIT or public domain\n")
file(WRITE
    "${WORK_DIR}/share/AzureRender/licenses/nlohmann-json-LICENSE.txt"
    "MIT License\n")

set(MANIFEST_FILE "${CMAKE_CURRENT_BINARY_DIR}/install-manifest-test-manifest.json")

# Positive: write manifest then verify it.
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DINSTALL_DIR=${WORK_DIR}
        -DOUTPUT_FILE=${MANIFEST_FILE}
        -P "${SOURCE_DIR}/tools/write_install_manifest.cmake"
    RESULT_VARIABLE WRITE_RESULT)
if(NOT WRITE_RESULT EQUAL 0)
    message(FATAL_ERROR "manifest write failed")
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DINSTALL_DIR=${WORK_DIR}
        -DMANIFEST_FILE=${MANIFEST_FILE}
        -P "${SOURCE_DIR}/tools/verify_install_manifest.cmake"
    RESULT_VARIABLE VERIFY_RESULT)
if(NOT VERIFY_RESULT EQUAL 0)
    message(FATAL_ERROR "manifest verification failed on an intact tree")
endif()

# Negative: corrupt a file, verification must fail.
file(APPEND "${WORK_DIR}/bin/AzureRender.exe" "tampered")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DINSTALL_DIR=${WORK_DIR}
        -DMANIFEST_FILE=${MANIFEST_FILE}
        -P "${SOURCE_DIR}/tools/verify_install_manifest.cmake"
    RESULT_VARIABLE TAMPER_RESULT)
if(TAMPER_RESULT EQUAL 0)
    message(FATAL_ERROR "manifest verification accepted a tampered tree")
endif()

file(REMOVE_RECURSE "${WORK_DIR}")
message(STATUS "Install manifest round-trip test passed")
