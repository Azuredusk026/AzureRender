if(NOT DEFINED PACKAGE_FILE OR NOT EXISTS "${PACKAGE_FILE}")
    message(FATAL_ERROR "PACKAGE_FILE must name an existing RC archive")
endif()
if(NOT DEFINED OUTPUT_FILE)
    set(OUTPUT_FILE "${PACKAGE_FILE}.manifest.json")
endif()

file(SHA256 "${PACKAGE_FILE}" PACKAGE_SHA256)
file(SIZE "${PACKAGE_FILE}" PACKAGE_SIZE)
get_filename_component(PACKAGE_NAME "${PACKAGE_FILE}" NAME)
string(TIMESTAMP GENERATED_AT "%Y-%m-%dT%H:%M:%SZ" UTC)
file(WRITE "${OUTPUT_FILE}"
    "{\n"
    "  \"schema_version\": 1,\n"
    "  \"package\": \"${PACKAGE_NAME}\",\n"
    "  \"size_bytes\": ${PACKAGE_SIZE},\n"
    "  \"sha256\": \"${PACKAGE_SHA256}\",\n"
    "  \"generated_at\": \"${GENERATED_AT}\"\n"
    "}\n")
message(STATUS "RC manifest: ${OUTPUT_FILE}")
