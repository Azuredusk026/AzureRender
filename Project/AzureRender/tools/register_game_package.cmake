function(azure_register_game_package)
    if(WIN32 AND (CMAKE_CONFIGURATION_TYPES OR CMAKE_BUILD_TYPE STREQUAL "Release"))
        add_test(NAME AzureEngine.GamePackage
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/test_game_package.py"
            --build-dir "${CMAKE_CURRENT_BINARY_DIR}" --editor $<TARGET_FILE:AzureRender>
            --output "${CMAKE_CURRENT_BINARY_DIR}/game-package/Windows Game"
            --evidence "${CMAKE_CURRENT_BINARY_DIR}/game-package/evidence"
            CONFIGURATIONS Release)
        set_tests_properties(AzureEngine.GamePackage PROPERTIES
            LABELS "gpu;release" RUN_SERIAL TRUE TIMEOUT 240)
    endif()
endfunction()
