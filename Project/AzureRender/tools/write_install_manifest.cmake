# Writes a machine-readable content manifest for a staged install tree.
#
# Usage:
#   cmake -DINSTALL_DIR=<path> -DOUTPUT_FILE=<path> -P tools/write_install_manifest.cmake
#
# The manifest lists every regular file under INSTALL_DIR with its size and
# SHA-256, plus the set of expected license files. It is the reproducible
# artifact inventory used by the RC release audit (AR-5.5).

if(NOT DEFINED INSTALL_DIR OR NOT IS_DIRECTORY "${INSTALL_DIR}")
    message(FATAL_ERROR "INSTALL_DIR must name an existing directory")
endif()
if(NOT DEFINED OUTPUT_FILE)
    set(OUTPUT_FILE "${INSTALL_DIR}/install_manifest.json")
endif()
file(TO_CMAKE_PATH "${OUTPUT_FILE}" OUTPUT_FILE_CMAKE)

file(GLOB_RECURSE INSTALLED_FILES
    RELATIVE "${INSTALL_DIR}"
    "${INSTALL_DIR}/*")

set(MANIFEST_LINES
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"files\": [\n")
set(ENTRY_COUNT 0)
foreach(FILE_PATH IN LISTS INSTALLED_FILES)
    set(FULL_PATH "${INSTALL_DIR}/${FILE_PATH}")
    if(IS_DIRECTORY "${FULL_PATH}")
        continue()
    endif()
    # Never list the manifest document itself.
    if(FULL_PATH STREQUAL OUTPUT_FILE_CMAKE)
        continue()
    endif()
    file(SHA256 "${FULL_PATH}" FILE_SHA256)
    file(SIZE "${FULL_PATH}" FILE_SIZE)
    # Manual JSON escaping (CMake 3.20 lacks string(JSON ... ESCAPE)).
    string(REPLACE "\\" "\\\\" FILE_PATH_JSON "${FILE_PATH}")
    string(REPLACE "\"" "\\\"" FILE_PATH_JSON "${FILE_PATH_JSON}")
    string(REPLACE "\n" "\\n" FILE_PATH_JSON "${FILE_PATH_JSON}")
    string(REPLACE "\t" "\\t" FILE_PATH_JSON "${FILE_PATH_JSON}")
    if(ENTRY_COUNT GREATER 0)
        string(APPEND MANIFEST_LINES ",\n")
    endif()
    string(APPEND MANIFEST_LINES
        "    {\"path\": \"${FILE_PATH_JSON}\", \"size_bytes\": ${FILE_SIZE}, "
        "\"sha256\": \"${FILE_SHA256}\"}")
    math(EXPR ENTRY_COUNT "${ENTRY_COUNT} + 1")
endforeach()
string(APPEND MANIFEST_LINES
    "\n  ],\n"
    "  \"file_count\": ${ENTRY_COUNT},\n"
    "  \"license_files\": [\n")

# Expected third-party license texts bundled with the install.
set(LICENSE_DIR "${INSTALL_DIR}/share/AzureRender/licenses")
set(EXPECTED_LICENSES
    imgui-LICENSE.txt
    glfw3-LICENSE.txt
    tinygltf-LICENSE.txt
    stb-LICENSE.txt
    nlohmann-json-LICENSE.txt)
set(LICENSE_COUNT 0)
foreach(LICENSE IN LISTS EXPECTED_LICENSES)
    if(EXISTS "${LICENSE_DIR}/${LICENSE}")
        string(REPLACE "\\" "\\\\" LICENSE_JSON "${LICENSE}")
        string(REPLACE "\"" "\\\"" LICENSE_JSON "${LICENSE_JSON}")
        if(LICENSE_COUNT GREATER 0)
            string(APPEND MANIFEST_LINES ",\n")
        endif()
        string(APPEND MANIFEST_LINES "    \"${LICENSE_JSON}\"")
        math(EXPR LICENSE_COUNT "${LICENSE_COUNT} + 1")
    endif()
endforeach()
string(APPEND MANIFEST_LINES
    "\n  ],\n"
    "  \"license_count\": ${LICENSE_COUNT}\n"
    "}\n")

file(WRITE "${OUTPUT_FILE}" "${MANIFEST_LINES}")
message(STATUS
    "Install manifest: ${OUTPUT_FILE} (${ENTRY_COUNT} files, "
    "${LICENSE_COUNT} licenses)")
