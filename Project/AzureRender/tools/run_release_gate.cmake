cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED SOURCE_DIR)
    get_filename_component(SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
if(NOT DEFINED BUILD_DIR)
    message(FATAL_ERROR "BUILD_DIR is required")
endif()
get_filename_component(BUILD_DIR "${BUILD_DIR}" ABSOLUTE)
if(NOT IS_DIRECTORY "${BUILD_DIR}" OR NOT EXISTS "${BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "Release gate requires a configured build directory: ${BUILD_DIR}")
endif()
if(NOT DEFINED CONFIG)
    set(CONFIG Release)
endif()

set(GATE_DIR "${BUILD_DIR}/release-gate")
set(INSTALL_DIR "${GATE_DIR}/install")
set(MOVED_DIR "${GATE_DIR}/install-moved")
set(RESULT_FILE "${GATE_DIR}/result.json")
file(REMOVE_RECURSE "${GATE_DIR}")
file(MAKE_DIRECTORY "${GATE_DIR}")

function(run_gate_stage NAME)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE STAGE_RESULT
        OUTPUT_VARIABLE STAGE_OUTPUT
        ERROR_VARIABLE STAGE_ERROR)
    file(WRITE "${GATE_DIR}/${NAME}.log" "${STAGE_OUTPUT}${STAGE_ERROR}")
    if(NOT STAGE_RESULT EQUAL 0)
        file(WRITE "${RESULT_FILE}"
            "{\n  \"schema_version\": 1,\n  \"status\": \"failed\",\n"
            "  \"failed_stage\": \"${NAME}\"\n}\n")
        message(FATAL_ERROR "Release gate stage '${NAME}' failed; see ${GATE_DIR}/${NAME}.log")
    endif()
endfunction()

run_gate_stage(configure
    "${CMAKE_COMMAND}" -S "${SOURCE_DIR}" -B "${BUILD_DIR}")
run_gate_stage(build
    "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${CONFIG}")
run_gate_stage(test
    "${CMAKE_CTEST_COMMAND}" --test-dir "${BUILD_DIR}" -C "${CONFIG}"
    --output-on-failure)
run_gate_stage(install
    "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --config "${CONFIG}"
    --prefix "${INSTALL_DIR}")
file(RENAME "${INSTALL_DIR}" "${MOVED_DIR}")

if(WIN32)
    set(INSTALLED_EXECUTABLE "${MOVED_DIR}/bin/AzureRender.exe")
else()
    set(INSTALLED_EXECUTABLE "${MOVED_DIR}/bin/AzureRender")
endif()
run_gate_stage(version "${INSTALLED_EXECUTABLE}" --version)
run_gate_stage(resources "${INSTALLED_EXECUTABLE}" --check-resources)

run_gate_stage(package
    "${CMAKE_CPACK_COMMAND}" -G TGZ -C "${CONFIG}"
    -B "${BUILD_DIR}" --config "${BUILD_DIR}/CPackConfig.cmake")
include("${BUILD_DIR}/CPackConfig.cmake")
set(PACKAGE_FILE "${BUILD_DIR}/${CPACK_PACKAGE_FILE_NAME}.tar.gz")
if(NOT EXISTS "${PACKAGE_FILE}")
    message(FATAL_ERROR "Release gate package was not generated: ${PACKAGE_FILE}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar tf "${PACKAGE_FILE}"
    RESULT_VARIABLE LIST_RESULT
    OUTPUT_VARIABLE PACKAGE_CONTENTS)
if(NOT LIST_RESULT EQUAL 0
    OR PACKAGE_CONTENTS MATCHES "assets_private"
    OR PACKAGE_CONTENTS MATCHES "captures/")
    message(FATAL_ERROR "Release package content/privacy check failed")
endif()

set(MANIFEST_FILE "${PACKAGE_FILE}.manifest.json")
run_gate_stage(manifest
    "${CMAKE_COMMAND}" -DPACKAGE_FILE=${PACKAGE_FILE}
    -DOUTPUT_FILE=${MANIFEST_FILE}
    -P "${SOURCE_DIR}/tools/write_rc_manifest.cmake")
file(SHA256 "${PACKAGE_FILE}" PACKAGE_SHA256)
file(SIZE "${PACKAGE_FILE}" PACKAGE_SIZE)
file(TO_CMAKE_PATH "${PACKAGE_FILE}" PACKAGE_JSON_PATH)
file(WRITE "${RESULT_FILE}"
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"status\": \"passed\",\n"
    "  \"configuration\": \"${CONFIG}\",\n"
    "  \"package\": \"${PACKAGE_JSON_PATH}\",\n"
    "  \"size_bytes\": ${PACKAGE_SIZE},\n"
    "  \"sha256\": \"${PACKAGE_SHA256}\",\n"
    "  \"stages\": [\"configure\", \"build\", \"test\", \"install\", \"version\", \"resources\", \"package\", \"manifest\"]\n"
    "}\n")
message(STATUS "Release gate passed: ${RESULT_FILE}")
