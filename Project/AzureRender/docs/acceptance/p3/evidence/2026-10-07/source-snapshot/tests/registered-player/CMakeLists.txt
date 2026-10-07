cmake_minimum_required(VERSION 3.20)
project(AzureRegisteredPlayerFixture LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/EHsc /utf-8)
endif()

get_filename_component(AZURE_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(BUILD_TESTING OFF CACHE BOOL "Fixture uses the engine's runtime targets" FORCE)
add_subdirectory("${AZURE_SOURCE_DIR}" engine)
add_executable(AzureRegisteredPlayer
    "${AZURE_SOURCE_DIR}/src/player/main.cpp"
    "${AZURE_SOURCE_DIR}/tests/RegisterHostExtension.cpp")
target_link_libraries(AzureRegisteredPlayer PRIVATE AzurePlayerHost)
add_dependencies(AzureRegisteredPlayer AzureRenderShaders)
