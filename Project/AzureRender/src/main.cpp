#include "app/AzureRenderApp.hpp"

#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int main(const int argumentCount, char** argumentValues) {
    try {
        AzureRenderOptions options;
        for (int index = 1; index < argumentCount; ++index) {
            const std::string argument = argumentValues[index];
            if (argument == "--smoke-frames" && index + 1 < argumentCount) {
                options.smokeFrameLimit = std::stoull(argumentValues[++index]);
                if (options.smokeFrameLimit == 0) {
                    throw std::invalid_argument("--smoke-frames must be greater than zero");
                }
            } else if (argument == "--asset" && index + 1 < argumentCount) {
                options.assetPath = argumentValues[++index];
            } else if (argument == "--capture-dir"
                       && index + 1 < argumentCount) {
                options.captureDirectory = argumentValues[++index];
            } else if (argument == "--capture-frames"
                       && index + 1 < argumentCount) {
                options.captureFrameLimit =
                    std::stoull(argumentValues[++index]);
                if (options.captureFrameLimit == 0) {
                    throw std::invalid_argument(
                        "--capture-frames must be greater than zero");
                }
            } else if (argument == "--capture-fps"
                       && index + 1 < argumentCount) {
                options.captureFps = static_cast<std::uint32_t>(
                    std::stoul(argumentValues[++index]));
                if (options.captureFps == 0 || options.captureFps > 240) {
                    throw std::invalid_argument(
                        "--capture-fps must be between 1 and 240");
                }
            } else if (argument == "--width"
                       && index + 1 < argumentCount) {
                options.width = static_cast<std::uint32_t>(
                    std::stoul(argumentValues[++index]));
            } else if (argument == "--height"
                       && index + 1 < argumentCount) {
                options.height = static_cast<std::uint32_t>(
                    std::stoul(argumentValues[++index]));
            } else if (argument == "--portfolio") {
                options.portfolioMode = true;
            } else if (argument == "--gpu-timing") {
                options.gpuTimingEnabled = true;
            } else if (argument == "--gpu-timing-output"
                       && index + 1 < argumentCount) {
                options.gpuTimingEnabled = true;
                options.gpuTimingOutput = argumentValues[++index];
            } else if (argument == "--diagnostic-view"
                       && index + 1 < argumentCount) {
                const std::string view = argumentValues[++index];
                if (view == "beauty") {
                    options.diagnosticView = 0;
                } else if (view == "normal") {
                    options.diagnosticView = 1;
                } else if (view == "outline") {
                    options.diagnosticView = 2;
                } else if (view == "shadow") {
                    options.diagnosticView = 3;
                } else {
                    throw std::invalid_argument(
                        "--diagnostic-view must be beauty, normal, "
                        "outline, or shadow");
                }
            } else if (argument == "--no-stylized") {
                options.stylizedLightingEnabled = false;
            } else if (argument == "--no-inner-outline") {
                options.innerOutlineEnabled = false;
            } else if (argument == "--hud") {
                options.hudEnabled = true;
                options.gpuTimingEnabled = true;
            } else if (argument == "--technical-sequence") {
                options.technicalSequence = true;
                options.portfolioMode = true;
                options.gpuTimingEnabled = true;
            } else {
                throw std::invalid_argument(
                    "Usage: AzureRender.exe [--asset <gltf/glb path>] "
                    "[--smoke-frames <positive integer>] "
                    "[--portfolio] [--width <pixels>] [--height <pixels>] "
                    "[--gpu-timing] [--gpu-timing-output <json path>] "
                    "[--diagnostic-view <beauty|normal|outline|shadow>] "
                    "[--no-stylized] [--no-inner-outline] [--hud] "
                    "[--technical-sequence] "
                    "[--capture-dir <empty directory> "
                    "--capture-frames <positive integer> "
                    "--capture-fps <1-240>]");
            }
        }
        if (options.width < 64 || options.width > 7680
            || options.height < 64 || options.height > 4320) {
            throw std::invalid_argument(
                "--width/--height must be within 64x64 and 7680x4320");
        }
        const bool hasCaptureDirectory =
            !options.captureDirectory.empty();
        const bool hasCaptureFrames = options.captureFrameLimit > 0;
        if (hasCaptureDirectory != hasCaptureFrames) {
            throw std::invalid_argument(
                "--capture-dir and --capture-frames must be used together");
        }
        if (options.technicalSequence
            && options.captureFrameLimit < 5) {
            throw std::invalid_argument(
                "--technical-sequence requires capture with at least 5 frames");
        }
        if (options.technicalSequence
            && options.captureFrameLimit % 5 != 0) {
            throw std::invalid_argument(
                "--technical-sequence capture frames must be divisible by 5");
        }

        AzureRenderApp application;
        application.run(options);
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
