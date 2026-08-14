#include "app/CommandLine.hpp"

#include <cstdint>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using azurerender::CommandLineError;
using azurerender::CommandLineErrorCode;

void require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void expectError(
    const CommandLineErrorCode expected,
    const std::string& option,
    const std::initializer_list<const char*> arguments) {
    try {
        std::vector<std::string> values;
        for (const char* argument : arguments) values.emplace_back(argument);
        (void)azurerender::parseCommandLine(values);
        throw std::runtime_error("Expected command-line parsing to fail");
    } catch (const CommandLineError& error) {
        require(error.code() == expected, "Unexpected command-line error code");
        require(error.option() == option, "Unexpected command-line error option");
    }
}

}  // namespace

int main() {
    const auto defaults = azurerender::parseCommandLine({});
    require(defaults.options.width == 1280, "Unexpected default width");
    require(defaults.options.height == 720, "Unexpected default height");
    require(defaults.options.captureFps == 60, "Unexpected default capture FPS");

    const auto valid = azurerender::parseCommandLine({
        "--asset", "demo.gltf",
        "--create-scene", "demo.azscene",
        "--width", "1920",
        "--height", "1080",
        "--capture-dir", "capture",
        "--capture-frames", "10",
        "--capture-fps", "30",
        "--diagnostic-view", "normal",
        "--hud"});
    require(
        valid.options.assetPath == "demo.gltf",
        "Asset path was not parsed");
    require(
        valid.createScenePath == "demo.azscene",
        "Scene path was not parsed");
    require(valid.options.width == 1920, "Width was not parsed");
    require(valid.options.height == 1080, "Height was not parsed");
    require(
        valid.options.captureFrameLimit == 10,
        "Frame count was not parsed");
    require(valid.options.captureFps == 30, "Capture FPS was not parsed");
    require(
        valid.options.renderSettings.diagnosticView == 1,
        "Diagnostic view was not parsed");
    require(valid.options.hudEnabled, "HUD flag was not parsed");
    require(valid.options.gpuTimingEnabled, "GPU timing was not implied by HUD");

    expectError(
        CommandLineErrorCode::UnknownOption,
        "--unknown",
        {"--unknown"});
    expectError(
        CommandLineErrorCode::MissingValue,
        "--asset",
        {"--asset"});
    expectError(
        CommandLineErrorCode::MissingValue,
        "--asset",
        {"--asset", "--version"});
    expectError(
        CommandLineErrorCode::MissingValue,
        "--asset",
        {"--asset", ""});
    expectError(
        CommandLineErrorCode::InvalidValue,
        "--width",
        {"--width", "1280px"});
    expectError(
        CommandLineErrorCode::InvalidValue,
        "--width",
        {"--width", "4294967296"});
    expectError(
        CommandLineErrorCode::InvalidValue,
        "--smoke-frames",
        {"--smoke-frames", "18446744073709551616"});
    expectError(
        CommandLineErrorCode::InvalidValue,
        "--smoke-frames",
        {"--smoke-frames", "-1"});
    expectError(
        CommandLineErrorCode::InvalidValue,
        "--smoke-frames",
        {"--smoke-frames", "0"});
    expectError(
        CommandLineErrorCode::InvalidValue,
        "--capture-fps",
        {"--capture-fps", "241"});
    expectError(
        CommandLineErrorCode::InvalidCombination,
        "--capture-dir/--capture-frames",
        {"--capture-dir", "capture"});
    expectError(
        CommandLineErrorCode::InvalidCombination,
        "--create-scene",
        {"--create-scene", "demo.azscene"});
    expectError(
        CommandLineErrorCode::InvalidCombination,
        "--scene/--create-scene/--editor",
        {"--scene", "a.azscene", "--editor", "b.azscene"});
    expectError(
        CommandLineErrorCode::InvalidCombination,
        "--qa-effect-state",
        {"--qa-effect-state", "enabled"});
    expectError(
        CommandLineErrorCode::InvalidValue,
        "--qa-camera",
        {"--qa-camera", "sideways"});
    expectError(
        CommandLineErrorCode::InvalidValue,
        "--qa-isolation",
        {"--qa-isolation", "unknown"});

    require(
        std::string(azurerender::commandLineUsage()).find("Usage:") == 0,
        "Usage contract is missing");
    return 0;
}
