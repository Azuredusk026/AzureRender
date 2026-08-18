#include "CommandLine.hpp"

#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <system_error>
#include <utility>

namespace azurerender {

namespace {

constexpr const char* kUsage =
    "Usage: AzureRender.exe [--help] [--asset <gltf/glb path>] [--version] "
    "[--check-resources] [--resource-root <directory>] "
    "[--environment <hdr/png/jpg path or cubemap directory>] "
    "[--scene <azscene path>] [--create-scene <azscene path>] "
    "[--editor <azscene path>] [--smoke-frames <positive integer>] "
    "[--portfolio] [--width <pixels>] [--height <pixels>] "
    "[--gpu-timing] [--gpu-timing-output <json path>] "
    "[--diagnostic-view <beauty|normal|outline|shadow>] "
    "[--no-stylized] [--no-inner-outline] [--hud] "
    "[--technical-sequence] "
    "[--qa-camera <preset>] [--qa-light <preset>] "
    "[--qa-effect <effect> --qa-effect-state <state>] "
    "[--qa-isolation <view>] [--render-path <traditional|subpasses|dynamic>] "
    "[--scene-type <character|blackhole>] "
    "[--blackhole-quality <performance|balanced|cinematic>] "
    "[--blackhole-camera <front|orbit-left|high|close|over-shoulder>] "
    "[--capture-dir <empty directory> "
    "--capture-frames <positive integer> --capture-fps <1-240>]";

constexpr const char* kHelp =
    "AzureRender 0.1 command-line reference\n\n"
    "Scenes:\n"
    "  --scene-type character|blackhole|sample\n"
    "                                      Select the renderer\n"
    "  --asset <gltf/glb>                  Character asset\n"
    "  --scene <azscene>                   Load a saved scene\n"
    "  --editor <azscene>                  Open the editor\n\n"
    "Output:\n"
    "  --width <pixels> --height <pixels>  Output size\n"
    "  --capture-dir <empty-dir>           Deterministic PNG output\n"
    "  --capture-frames <N> --capture-fps <1-240>\n"
    "  --gpu-timing-output <json>          GPU pass timing\n\n"
    "Quality and QA:\n"
    "  --qa-camera <preset> --qa-light <preset>\n"
    "  --blackhole-quality performance|balanced|cinematic\n"
    "  --blackhole-camera front|orbit-left|high|close|over-shoulder\n"
    "  --diagnostic-view beauty|normal|outline|shadow\n"
    "  --hud --no-stylized --no-inner-outline\n\n"
    "Utility:\n"
    "  --check-resources  Validate the installed resource tree\n"
    "  --smoke-frames <N> Exit after N rendered frames\n"
    "  --version          Print the version\n"
    "  --help             Print this reference\n";

[[noreturn]] void fail(
    const CommandLineErrorCode code,
    const std::string& option,
    const std::string& message) {
    throw CommandLineError(code, option, message);
}

const std::string& requireValue(
    const std::vector<std::string>& arguments,
    std::size_t& index,
    const std::string& option) {
    if (index + 1 >= arguments.size()
        || arguments[index + 1].empty()
        || arguments[index + 1].rfind("--", 0) == 0) {
        fail(
            CommandLineErrorCode::MissingValue,
            option,
            option + " requires a value");
    }
    return arguments[++index];
}

bool isOneOf(
    const std::string& value,
    const std::initializer_list<const char*> accepted) {
    for (const char* candidate : accepted) {
        if (value == candidate) return true;
    }
    return false;
}

std::uint64_t parseUnsigned(
    const std::string& value,
    const std::string& option) {
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), parsed);
    if (value.empty() || result.ec != std::errc{}
        || result.ptr != value.data() + value.size()) {
        fail(
            CommandLineErrorCode::InvalidValue,
            option,
            option + " requires an unsigned integer");
    }
    return parsed;
}

std::uint32_t parseUint32(
    const std::string& value,
    const std::string& option) {
    const std::uint64_t parsed = parseUnsigned(value, option);
    if (parsed > std::numeric_limits<std::uint32_t>::max()) {
        fail(
            CommandLineErrorCode::InvalidValue,
            option,
            option + " is outside the supported range");
    }
    return static_cast<std::uint32_t>(parsed);
}

void validate(const ParsedCommandLine& parsed) {
    const auto& options = parsed.options;
    if (options.width < 64 || options.width > 7680
        || options.height < 64 || options.height > 4320) {
        fail(
            CommandLineErrorCode::InvalidValue,
            "--width/--height",
            "--width/--height must be within 64x64 and 7680x4320");
    }
    const bool hasCaptureDirectory = !options.captureDirectory.empty();
    const bool hasCaptureFrames = options.captureFrameLimit > 0;
    if (hasCaptureDirectory != hasCaptureFrames) {
        fail(
            CommandLineErrorCode::InvalidCombination,
            "--capture-dir/--capture-frames",
            "--capture-dir and --capture-frames must be used together");
    }
    if (options.technicalSequence && options.captureFrameLimit < 5) {
        fail(
            CommandLineErrorCode::InvalidCombination,
            "--technical-sequence",
            "--technical-sequence requires capture with at least 5 frames");
    }
    if (options.technicalSequence && options.captureFrameLimit % 5 != 0) {
        fail(
            CommandLineErrorCode::InvalidCombination,
            "--technical-sequence",
            "--technical-sequence capture frames must be divisible by 5");
    }
    if (!options.qaEffectState.empty() && options.qaEffect.empty()) {
        fail(
            CommandLineErrorCode::InvalidCombination,
            "--qa-effect-state",
            "--qa-effect-state requires --qa-effect");
    }
    if (!options.qaCamera.empty()
        && !isOneOf(options.qaCamera, {
            "full-body-front", "face-front", "face-three-quarter",
            "back-detail", "lighting-sweep"})) {
        fail(CommandLineErrorCode::InvalidValue, "--qa-camera",
             "Unknown --qa-camera: " + options.qaCamera);
    }
    if (!options.renderPathName.empty()
        && !isOneOf(options.renderPathName, {
            "traditional", "subpasses", "dynamic"})) {
        fail(CommandLineErrorCode::InvalidValue, "--render-path",
             "Unknown --render-path: " + options.renderPathName);
    }
    if (!options.qaLight.empty()
        && !isOneOf(options.qaLight, {
            "neutral-material", "stylized-key", "specular-rim",
            "rear-emissive"})) {
        fail(CommandLineErrorCode::InvalidValue, "--qa-light",
             "Unknown --qa-light: " + options.qaLight);
    }
    if (!options.qaEffect.empty()
        && !isOneOf(options.qaEffect, {
            "toon", "shadow", "hair-kk", "rim", "specular", "emissive",
            "outline", "face-sdf", "overlay", "bloom"})) {
        fail(CommandLineErrorCode::InvalidValue, "--qa-effect",
             "Unknown --qa-effect: " + options.qaEffect);
    }
    if (!options.qaEffectState.empty()
        && !isOneOf(options.qaEffectState, {
            "enabled", "disabled", "isolation"})) {
        fail(CommandLineErrorCode::InvalidValue, "--qa-effect-state",
             "Unknown --qa-effect-state: " + options.qaEffectState);
    }
    if (!options.qaIsolation.empty()
        && !isOneOf(options.qaIsolation, {
            "beauty", "albedo", "world-normal", "depth", "diffuse-band",
            "shadow-visibility", "hair-kk", "rim", "specular", "emissive",
            "outline", "shadow-map", "material-id", "style-mask", "ambient",
            "direct-diffuse", "shadow-tint", "face-sdf", "overlay",
            "bloom"})) {
        fail(CommandLineErrorCode::InvalidValue, "--qa-isolation",
             "Unknown --qa-isolation: " + options.qaIsolation);
    }
    const bool qaRequested = !options.qaCamera.empty()
        || !options.qaLight.empty()
        || !options.qaEffect.empty()
        || !options.qaEffectState.empty()
        || !options.qaIsolation.empty();
    if (qaRequested && options.technicalSequence) {
        fail(
            CommandLineErrorCode::InvalidCombination,
            "--qa-*",
            "CQ-0 --qa-* options cannot be combined with --technical-sequence");
    }
    const int sceneModes = static_cast<int>(!parsed.scenePath.empty())
        + static_cast<int>(!parsed.createScenePath.empty())
        + static_cast<int>(!parsed.editorScenePath.empty());
    if (sceneModes > 1) {
        fail(
            CommandLineErrorCode::InvalidCombination,
            "--scene/--create-scene/--editor",
            "--scene, --create-scene, and --editor are mutually exclusive");
    }
    if (!parsed.createScenePath.empty() && options.assetPath.empty()) {
        fail(
            CommandLineErrorCode::InvalidCombination,
            "--create-scene",
            "--create-scene requires --asset");
    }
}

}  // namespace

CommandLineError::CommandLineError(
    const CommandLineErrorCode code,
    std::string option,
    std::string message)
    : std::invalid_argument(std::move(message)),
      code_(code),
      option_(std::move(option)) {}

const char* commandLineUsage() noexcept {
    return kUsage;
}

const char* commandLineHelp() noexcept {
    return kHelp;
}

ParsedCommandLine parseCommandLine(
    const std::vector<std::string>& arguments) {
    ParsedCommandLine parsed;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string& argument = arguments[index];
        if (argument == "--help") {
            parsed.showHelp = true;
        } else if (argument == "--smoke-frames") {
            parsed.options.smokeFrameLimit = parseUnsigned(
                requireValue(arguments, index, argument), argument);
            if (parsed.options.smokeFrameLimit == 0) {
                fail(CommandLineErrorCode::InvalidValue, argument,
                     "--smoke-frames must be greater than zero");
            }
        } else if (argument == "--version") {
            parsed.showVersion = true;
        } else if (argument == "--check-resources") {
            parsed.checkResources = true;
        } else if (argument == "--asset") {
            parsed.options.assetPath = requireValue(arguments, index, argument);
        } else if (argument == "--environment") {
            parsed.options.environmentPath =
                requireValue(arguments, index, argument);
        } else if (argument == "--resource-root") {
            parsed.options.resourceRoot =
                requireValue(arguments, index, argument);
        } else if (argument == "--scene") {
            parsed.scenePath = requireValue(arguments, index, argument);
        } else if (argument == "--create-scene") {
            parsed.createScenePath = requireValue(arguments, index, argument);
        } else if (argument == "--editor") {
            parsed.editorScenePath = requireValue(arguments, index, argument);
        } else if (argument == "--capture-dir") {
            parsed.options.captureDirectory =
                requireValue(arguments, index, argument);
        } else if (argument == "--capture-frames") {
            parsed.options.captureFrameLimit = parseUnsigned(
                requireValue(arguments, index, argument), argument);
            if (parsed.options.captureFrameLimit == 0) {
                fail(CommandLineErrorCode::InvalidValue, argument,
                     "--capture-frames must be greater than zero");
            }
        } else if (argument == "--capture-fps") {
            parsed.options.captureFps = parseUint32(
                requireValue(arguments, index, argument), argument);
            if (parsed.options.captureFps == 0
                || parsed.options.captureFps > 240) {
                fail(CommandLineErrorCode::InvalidValue, argument,
                     "--capture-fps must be between 1 and 240");
            }
        } else if (argument == "--width") {
            parsed.options.width = parseUint32(
                requireValue(arguments, index, argument), argument);
        } else if (argument == "--height") {
            parsed.options.height = parseUint32(
                requireValue(arguments, index, argument), argument);
        } else if (argument == "--portfolio") {
            parsed.options.portfolioMode = true;
        } else if (argument == "--gpu-timing") {
            parsed.options.gpuTimingEnabled = true;
        } else if (argument == "--gpu-timing-output") {
            parsed.options.gpuTimingEnabled = true;
            parsed.options.gpuTimingOutput =
                requireValue(arguments, index, argument);
        } else if (argument == "--diagnostic-view") {
            const std::string& view = requireValue(arguments, index, argument);
            if (view == "beauty") {
                parsed.options.renderSettings.diagnosticView = 0;
            } else if (view == "normal") {
                parsed.options.renderSettings.diagnosticView = 1;
            } else if (view == "outline") {
                parsed.options.renderSettings.diagnosticView = 2;
            } else if (view == "shadow") {
                parsed.options.renderSettings.diagnosticView = 3;
            } else {
                fail(
                    CommandLineErrorCode::InvalidValue,
                    argument,
                    "--diagnostic-view must be beauty, normal, outline, or shadow");
            }
        } else if (argument == "--no-stylized") {
            parsed.options.renderSettings.stylizedLightingEnabled = false;
        } else if (argument == "--no-inner-outline") {
            parsed.options.renderSettings.innerOutlineEnabled = false;
        } else if (argument == "--hud") {
            parsed.options.hudEnabled = true;
            parsed.options.gpuTimingEnabled = true;
        } else if (argument == "--technical-sequence") {
            parsed.options.technicalSequence = true;
            parsed.options.portfolioMode = true;
            parsed.options.gpuTimingEnabled = true;
        } else if (argument == "--qa-camera") {
            parsed.options.qaCamera = requireValue(arguments, index, argument);
        } else if (argument == "--qa-light") {
            parsed.options.qaLight = requireValue(arguments, index, argument);
        } else if (argument == "--qa-effect") {
            parsed.options.qaEffect = requireValue(arguments, index, argument);
        } else if (argument == "--qa-effect-state") {
            parsed.options.qaEffectState =
                requireValue(arguments, index, argument);
        } else if (argument == "--render-path") {
            parsed.options.renderPathName =
                requireValue(arguments, index, argument);
        } else if (argument == "--scene-type") {
            const std::string& sceneTypeName =
                requireValue(arguments, index, argument);
            try {
                parsed.options.renderSettings.sceneType =
                    azurerender::sceneTypeFromString(sceneTypeName);
            } catch (const std::invalid_argument&) {
                fail(
                    CommandLineErrorCode::InvalidValue,
                    argument,
                    "--scene-type must be character, blackhole or sample");
            }
        } else if (argument == "--blackhole-quality") {
            const std::string& value = requireValue(arguments, index, argument);
            if (value == "performance") {
                parsed.options.renderSettings.blackhole.quality =
                    BlackholeQuality::Performance;
            } else if (value == "balanced") {
                parsed.options.renderSettings.blackhole.quality =
                    BlackholeQuality::Balanced;
            } else if (value == "cinematic") {
                parsed.options.renderSettings.blackhole.quality =
                    BlackholeQuality::Cinematic;
            } else {
                fail(CommandLineErrorCode::InvalidValue, argument,
                    "--blackhole-quality must be performance, balanced or cinematic");
            }
        } else if (argument == "--blackhole-camera") {
            const std::string& value = requireValue(arguments, index, argument);
            if (value == "front") {
                parsed.options.renderSettings.blackhole.camera =
                    BlackholeCamera::Front;
            } else if (value == "orbit-left") {
                parsed.options.renderSettings.blackhole.camera =
                    BlackholeCamera::OrbitLeft;
            } else if (value == "high") {
                parsed.options.renderSettings.blackhole.camera =
                    BlackholeCamera::High;
            } else if (value == "close") {
                parsed.options.renderSettings.blackhole.camera =
                    BlackholeCamera::Close;
            } else if (value == "over-shoulder") {
                parsed.options.renderSettings.blackhole.camera =
                    BlackholeCamera::OverShoulder;
            } else {
                fail(CommandLineErrorCode::InvalidValue, argument,
                    "--blackhole-camera must be front, orbit-left, high, close or over-shoulder");
            }
        } else if (argument == "--qa-isolation") {
            parsed.options.qaIsolation = requireValue(arguments, index, argument);
        } else {
            fail(
                CommandLineErrorCode::UnknownOption,
                argument,
                "Unknown option: " + argument + "\n" + kUsage);
        }
    }
    validate(parsed);
    return parsed;
}

}  // namespace azurerender
