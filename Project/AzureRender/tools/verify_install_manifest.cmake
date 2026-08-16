# Verifies a staged install tree against its content manifest.
#
# Usage:
#   cmake -DINSTALL_DIR=<path> -DMANIFEST_FILE=<path> -P tools/verify_install_manifest.cmake
#
# Reproducibility: regenerates the manifest for the current tree and compares
# it with the recorded one. Any difference (missing/added file, changed size or
# SHA-256) fails verification. The third-party license texts required by
# AR-5.5 must be present.

if(NOT DEFINED INSTALL_DIR OR NOT IS_DIRECTORY "${INSTALL_DIR}")
    message(FATAL_ERROR "INSTALL_DIR must name an existing directory")
endif()
if(NOT DEFINED MANIFEST_FILE OR NOT EXISTS "${MANIFEST_FILE}")
    message(FATAL_ERROR "MANIFEST_FILE must name an existing manifest")
endif()

set(VIOLATIONS "")

# --- License completeness ------------------------------------------------
set(LICENSE_DIR "${INSTALL_DIR}/share/AzureRender/licenses")
set(EXPECTED_LICENSES
    imgui-LICENSE.txt
    glfw3-LICENSE.txt
    tinygltf-LICENSE.txt
    stb-LICENSE.txt
    nlohmann-json-LICENSE.txt)
foreach(LICENSE IN LISTS EXPECTED_LICENSES)
    if(NOT EXISTS "${LICENSE_DIR}/${LICENSE}")
        string(APPEND VIOLATIONS
            "missing license: share/AzureRender/licenses/${LICENSE}\n")
    endif()
endforeach()

# --- Reproducibility: regenerate and compare ------------------------------
# Write the regenerated manifest outside the install tree so it never
# influences the scan.
set(REGEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/verify-install-manifest")
file(MAKE_DIRECTORY "${REGEN_DIR}")
set(REGENERATED_FILE "${REGEN_DIR}/manifest.regenerated")
execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -DINSTALL_DIR=${INSTALL_DIR}
        -DOUTPUT_FILE=${REGENERATED_FILE}
        -P "${CMAKE_CURRENT_LIST_DIR}/write_install_manifest.cmake"
    RESULT_VARIABLE REGEN_RESULT)
if(NOT REGEN_RESULT EQUAL 0)
    message(FATAL_ERROR "manifest regeneration failed")
endif()

file(READ "${MANIFEST_FILE}" RECORDED_TEXT)
file(READ "${REGENERATED_FILE}" REGENERATED_TEXT)
file(REMOVE "${REGENERATED_FILE}")
file(REMOVE_RECURSE "${REGEN_DIR}")
if(NOT RECORDED_TEXT STREQUAL REGENERATED_TEXT)
    string(APPEND VIOLATIONS
        "tree contents differ from the recorded manifest "
        "(added/missing files or hash/size changes)\n")
endif()

if(NOT VIOLATIONS STREQUAL "")
    message(FATAL_ERROR
        "Install tree verification failed:\n${VIOLATIONS}")
endif()
message(STATUS "Install tree verified against manifest")
