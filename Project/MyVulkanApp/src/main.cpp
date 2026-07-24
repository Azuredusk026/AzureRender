#include "app/VulkanApp.hpp"

#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int main(const int argumentCount, char** argumentValues) {
    try {
        std::uint64_t smokeFrames = 0;
        std::string assetPath;
        for (int index = 1; index < argumentCount; ++index) {
            const std::string argument = argumentValues[index];
            if (argument == "--smoke-frames" && index + 1 < argumentCount) {
                smokeFrames = std::stoull(argumentValues[++index]);
                if (smokeFrames == 0) {
                    throw std::invalid_argument("--smoke-frames must be greater than zero");
                }
            } else if (argument == "--asset" && index + 1 < argumentCount) {
                assetPath = argumentValues[++index];
            } else {
                throw std::invalid_argument(
                    "Usage: MyVulkanApp.exe [--asset <gltf/glb path>] "
                    "[--smoke-frames <positive integer>]");
            }
        }

        VulkanApp application;
        application.run(assetPath, smokeFrames);
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
