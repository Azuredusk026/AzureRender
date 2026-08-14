#include "app/AzureRenderApp.hpp"
#include "editor/SceneModel.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

int main(const int argumentCount, char** argumentValues) {
    try {
        AzureRenderOptions options;
        std::string scenePath;
        std::string createScenePath;
        for (int index = 1; index < argumentCount; ++index) {
            const std::string argument = argumentValues[index];
            if (argument == "--smoke-frames" && index + 1 < argumentCount) {
                options.smokeFrameLimit = std::stoull(argumentValues[++index]);
                if (options.smokeFrameLimit == 0) {
                    throw std::invalid_argument("--smoke-frames must be greater than zero");
                }
            } else if (argument == "--asset" && index + 1 < argumentCount) {
                options.assetPath = argumentValues[++index];
            } else if (argument == "--scene" && index + 1 < argumentCount) {
                scenePath = argumentValues[++index];
            } else if (argument == "--create-scene"
                       && index + 1 < argumentCount) {
                createScenePath = argumentValues[++index];
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
                    options.renderSettings.diagnosticView = 0;
                } else if (view == "normal") {
                    options.renderSettings.diagnosticView = 1;
                } else if (view == "outline") {
                    options.renderSettings.diagnosticView = 2;
                } else if (view == "shadow") {
                    options.renderSettings.diagnosticView = 3;
                } else {
                    throw std::invalid_argument(
                        "--diagnostic-view must be beauty, normal, "
                        "outline, or shadow");
                }
            } else if (argument == "--no-stylized") {
                options.renderSettings.stylizedLightingEnabled = false;
            } else if (argument == "--no-inner-outline") {
                options.renderSettings.innerOutlineEnabled = false;
            } else if (argument == "--hud") {
                options.hudEnabled = true;
                options.gpuTimingEnabled = true;
            } else if (argument == "--technical-sequence") {
                options.technicalSequence = true;
                options.portfolioMode = true;
                options.gpuTimingEnabled = true;
            } else if (argument == "--qa-camera"
                       && index + 1 < argumentCount) {
                options.qaCamera = argumentValues[++index];
            } else if (argument == "--qa-light"
                       && index + 1 < argumentCount) {
                options.qaLight = argumentValues[++index];
            } else if (argument == "--qa-effect"
                       && index + 1 < argumentCount) {
                options.qaEffect = argumentValues[++index];
            } else if (argument == "--qa-effect-state"
                       && index + 1 < argumentCount) {
                options.qaEffectState = argumentValues[++index];
            } else if (argument == "--qa-isolation"
                       && index + 1 < argumentCount) {
                options.qaIsolation = argumentValues[++index];
            } else {
                throw std::invalid_argument(
                    "Usage: AzureRender.exe [--asset <gltf/glb path>] "
                    "[--scene <azscene path>] "
                    "[--create-scene <azscene path>] "
                    "[--smoke-frames <positive integer>] "
                    "[--portfolio] [--width <pixels>] [--height <pixels>] "
                    "[--gpu-timing] [--gpu-timing-output <json path>] "
                    "[--diagnostic-view <beauty|normal|outline|shadow>] "
                    "[--no-stylized] [--no-inner-outline] [--hud] "
                    "[--technical-sequence] "
                    "[--qa-camera <full-body-front|face-front|"
                    "face-three-quarter|back-detail|lighting-sweep>] "
                    "[--qa-light <neutral-material|stylized-key|"
                    "specular-rim|rear-emissive>] "
                    "[--qa-effect <toon|shadow|hair-kk|rim|specular|"
                    "emissive|outline|face-sdf|overlay|bloom> --qa-effect-state "
                    "<enabled|disabled|isolation>] "
                    "[--qa-isolation <beauty|albedo|world-normal|depth|"
                    "diffuse-band|shadow-visibility|hair-kk|rim|"
                    "specular|emissive|outline|shadow-map|material-id|"
                    "style-mask|ambient|direct-diffuse|shadow-tint|face-sdf|overlay|bloom>] "
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
        if (!options.qaEffectState.empty() && options.qaEffect.empty()) {
            throw std::invalid_argument(
                "--qa-effect-state requires --qa-effect");
        }
        const bool qaRequested = !options.qaCamera.empty()
            || !options.qaLight.empty()
            || !options.qaEffect.empty()
            || !options.qaEffectState.empty()
            || !options.qaIsolation.empty();
        if (qaRequested && options.technicalSequence) {
            throw std::invalid_argument(
                "CQ-0 --qa-* options cannot be combined with "
                "--technical-sequence");
        }

        if (!createScenePath.empty()) {
            if (options.assetPath.empty()) {
                throw std::invalid_argument(
                    "--create-scene requires --asset");
            }
            const azurerender::SceneDocument scene =
                azurerender::SceneDocument::fromAsset(options.assetPath);
            scene.save(createScenePath);
            std::cout << "Scene created: " << createScenePath << '\n';
            return EXIT_SUCCESS;
        }
        if (!scenePath.empty()) {
            const azurerender::SceneDocument scene =
                azurerender::SceneDocument::load(scenePath);
            const auto asset = std::find_if(
                scene.resources.begin(), scene.resources.end(),
                [](const azurerender::SceneResource& resource) {
                    return resource.type == "gltf";
                });
            if (asset == scene.resources.end()) {
                throw std::runtime_error(
                    "Scene contains no gltf resource: " + scenePath);
            }
            options.assetPath = asset->path.string();
            options.renderSettings = scene.renderSettings;
        }

        AzureRenderApp application;
        application.run(options);
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
