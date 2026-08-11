#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"

#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace azurerender::internal;

namespace {

std::string fnv1a64Hex(const std::string& value) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char byte : value) {
        hash ^= byte;
        hash *= 1099511628211ULL;
    }
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

}  // namespace

void AzureRenderApp::prepareCaptureDirectory() {
    const std::filesystem::path directory = runOptions_.captureDirectory;
    if (std::filesystem::exists(directory)) {
        if (!std::filesystem::is_directory(directory)) {
            throw std::runtime_error(
                "Capture path is not a directory: "
                + directory.string());
        }
        if (!std::filesystem::is_empty(directory)) {
            throw std::runtime_error(
                "Capture directory must be empty: "
                + directory.string());
        }
    } else if (!std::filesystem::create_directories(directory)) {
        throw std::runtime_error(
            "Failed to create capture directory: "
            + directory.string());
    }
}

void AzureRenderApp::writeCaptureManifest(
    const std::uint64_t renderedFrames) const {
    const std::filesystem::path outputPath =
        std::filesystem::path(runOptions_.captureDirectory)
        / "capture_manifest.json";
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        throw std::runtime_error(
            "Failed to create capture manifest: "
            + outputPath.string());
    }
    const std::string animationName =
        asset_.animations.empty()
        ? std::string()
        : asset_.animations[animationIndex_].name;
    constexpr std::array<const char*, 5> kDiagnosticNames = {
        "beauty",
        "world-normal",
        "internal-outline",
        "shadow-map",
        "depth",
    };
    std::ostringstream qaState;
    qaState
        << resolvedAssetPath_ << '|'
        << qaCameraName_ << '|'
        << qaLightName_ << '|'
        << qaEffectName_ << '|'
        << qaEffectStateName_ << '|'
        << qaIsolationName_ << '|'
        << diagnosticView_ << '|'
        << qaIsolationMode_ << '|'
        << qaEffectMode_ << '|'
        << qaEffectEnabled_ << '|'
        << showcasePreset_ << '|'
        << rotationAngle_ << '|'
        << cameraPosition_[0] << ',' << cameraPosition_[1] << ','
        << cameraPosition_[2] << '|'
        << cameraTarget_[0] << ',' << cameraTarget_[1] << ','
        << cameraTarget_[2] << '|'
        << stylizedLightingEnabled_ << '|'
        << innerOutlineEnabled_;
    for (const AssetMaterial& material : asset_.materials) {
        qaState
            << '|' << material.name
            << ':' << static_cast<std::uint32_t>(material.materialClass)
            << ':' << material.materialFeatures
            << ':' << material.materialProfileVersion
            << ':' << material.materialProfileExplicit;
        for (const float parameter : material.styleParameters) {
            qaState << ':' << parameter;
        }
        for (const float parameter : material.featureParameters) {
            qaState << ':' << parameter;
        }
    }
    const std::string qaStateHash = fnv1a64Hex(qaState.str());
    output
        << "{\n"
        << "  \"format\": \"Afterglow PNG sequence v1\",\n"
        << "  \"asset\": " << std::quoted(resolvedAssetPath_) << ",\n"
        << "  \"gpu\": " << std::quoted(selectedGpuName_) << ",\n"
        << "  \"width\": " << runOptions_.width << ",\n"
        << "  \"height\": " << runOptions_.height << ",\n"
        << "  \"fps\": " << runOptions_.captureFps << ",\n"
        << "  \"capturedFrames\": " << capturedFrames_ << ",\n"
        << "  \"renderedFrames\": " << renderedFrames << ",\n"
        << "  \"durationSeconds\": "
        << static_cast<double>(capturedFrames_)
               / static_cast<double>(runOptions_.captureFps)
        << ",\n"
        << "  \"portfolioMode\": "
        << (runOptions_.portfolioMode ? "true" : "false") << ",\n"
        << "  \"diagnosticView\": "
        << std::quoted(kDiagnosticNames[diagnosticView_]) << ",\n"
        << "  \"stylizedLighting\": "
        << (stylizedLightingEnabled_ ? "true" : "false") << ",\n"
        << "  \"internalOutline\": "
        << (innerOutlineEnabled_ ? "true" : "false") << ",\n"
        << "  \"hudEnabled\": "
        << (hudEnabled_ ? "true" : "false") << ",\n"
        << "  \"qaHarnessVersion\": \"CQ-0-v1\",\n"
        << "  \"qaHarnessEnabled\": "
        << (qaHarnessEnabled_ ? "true" : "false") << ",\n"
        << "  \"qaCamera\": " << std::quoted(qaCameraName_) << ",\n"
        << "  \"qaLight\": " << std::quoted(qaLightName_) << ",\n"
        << "  \"qaEffect\": " << std::quoted(qaEffectName_) << ",\n"
        << "  \"qaEffectState\": "
        << std::quoted(qaEffectStateName_) << ",\n"
        << "  \"qaIsolation\": "
        << std::quoted(qaIsolationName_) << ",\n"
        << "  \"qaStateHashAlgorithm\": \"FNV-1a-64\",\n"
        << "  \"qaStateHash\": " << std::quoted(qaStateHash) << ",\n"
        << "  \"qaCameraPosition\": ["
        << cameraPosition_[0] << ", " << cameraPosition_[1] << ", "
        << cameraPosition_[2] << "],\n"
        << "  \"qaCameraTarget\": ["
        << cameraTarget_[0] << ", " << cameraTarget_[1] << ", "
        << cameraTarget_[2] << "],\n"
        << "  \"qaModelRotationRadians\": " << rotationAngle_ << ",\n"
        << "  \"qaShowcasePreset\": " << showcasePreset_ << ",\n"
        << "  \"materialProfileSchemaVersion\": 1,\n"
        << "  \"materialInventory\": [\n";
    for (std::size_t index = 0; index < asset_.materials.size(); ++index) {
        const AssetMaterial& material = asset_.materials[index];
        std::size_t primitiveCount = 0;
        for (const AssetPrimitive& primitive : asset_.primitives) {
            if (primitive.materialIndex == index) {
                ++primitiveCount;
            }
        }
        output
            << "    {\"index\": " << index
            << ", \"name\": " << std::quoted(material.name)
            << ", \"class\": "
            << std::quoted(assetMaterialClassName(material.materialClass))
            << ", \"features\": " << material.materialFeatures
            << ", \"profileVersion\": "
            << material.materialProfileVersion
            << ", \"styleParameters\": ["
            << material.styleParameters[0] << ", "
            << material.styleParameters[1] << ", "
            << material.styleParameters[2] << ", "
            << material.styleParameters[3] << "]"
            << ", \"featureParameters\": ["
            << material.featureParameters[0] << ", "
            << material.featureParameters[1] << ", "
            << material.featureParameters[2] << ", "
            << material.featureParameters[3] << "]"
            << ", \"profileSource\": "
            << std::quoted(
                material.materialProfileExplicit
                    ? "asset-extras"
                    : "fallback/inferred")
            << ", \"primitiveCount\": " << primitiveCount << "}"
            << (index + 1 < asset_.materials.size() ? "," : "")
            << '\n';
    }
    output
        << "  ],\n"
        << "  \"technicalSequence\": "
        << (runOptions_.technicalSequence ? "true" : "false");
    if (runOptions_.technicalSequence) {
        constexpr std::array<const char*, 5> kChapterNames = {
            "beauty",
            "world-normal",
            "internal-outline",
            "shadow-map",
            "beauty-hud",
        };
        const std::uint64_t chapterFrames =
            runOptions_.captureFrameLimit / 5;
        const std::uint64_t fadeFrames =
            std::min<std::uint64_t>(
                chapterFrames / 3,
                std::max<std::uint64_t>(
                    1,
                    static_cast<std::uint64_t>(
                        runOptions_.captureFps * 35 / 100)));
        const std::uint64_t titleFrames =
            std::min<std::uint64_t>(
                chapterFrames,
                static_cast<std::uint64_t>(
                    runOptions_.captureFps * 2));
        output
            << ",\n  \"technicalFadeFrames\": "
            << fadeFrames
            << ",\n  \"technicalTitleFrames\": "
            << titleFrames
            << ",\n  \"technicalChapters\": [\n";
        for (std::size_t chapter = 0;
             chapter < kChapterNames.size();
             ++chapter) {
            output
                << "    {\"name\": "
                << std::quoted(kChapterNames[chapter])
                << ", \"startFrame\": "
                << chapter * chapterFrames
                << ", \"endFrameExclusive\": "
                << (chapter + 1) * chapterFrames
                << "}"
                << (chapter + 1 < kChapterNames.size() ? "," : "")
                << '\n';
        }
        output << "  ],\n";
    } else {
        output << ",\n";
    }
    output
        << "  \"animationIndex\": " << animationIndex_ << ",\n"
        << "  \"animation\": " << std::quoted(animationName) << ",\n"
        << "  \"framePattern\": \"frame_%06d.png\"\n"
        << "}\n";
    if (!output) {
        throw std::runtime_error(
            "Failed to write capture manifest: "
            + outputPath.string());
    }
    std::cout << "Capture manifest: " << outputPath.string() << '\n';
}


void AzureRenderApp::collectGpuTiming(const std::size_t frameIndex) {
    if (!runOptions_.gpuTimingEnabled
        || !timestampQuerySubmitted_[frameIndex]) {
        return;
    }
    std::array<std::uint64_t, kTimestampQueryCount> timestamps{};
    vkCheck(
        vkGetQueryPoolResults(
            device_,
            timestampQueryPools_[frameIndex],
            0,
            kTimestampQueryCount,
            sizeof(timestamps),
            timestamps.data(),
            sizeof(std::uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
        "vkGetQueryPoolResults(timestamp)");
    timestampQuerySubmitted_[frameIndex] = false;

    const std::uint64_t mask = timestampValidBits_ >= 64
        ? std::numeric_limits<std::uint64_t>::max()
        : (std::uint64_t{1} << timestampValidBits_) - 1;
    const auto elapsedMilliseconds =
        [&](const std::uint64_t begin, const std::uint64_t end) {
        const std::uint64_t ticks = (end - begin) & mask;
        return static_cast<double>(ticks)
            * timestampPeriodNanoseconds_ * 1.0e-6;
    };
    const double shadowMs =
        elapsedMilliseconds(timestamps[0], timestamps[1]);
    const double sceneMs =
        elapsedMilliseconds(timestamps[1], timestamps[2]);
    const double postProcessMs =
        elapsedMilliseconds(timestamps[2], timestamps[3]);
    const double frameMs =
        elapsedMilliseconds(timestamps[0], timestamps[3]);

    ++gpuTiming_.samples;
    gpuTiming_.shadowTotalMs += shadowMs;
    gpuTiming_.sceneTotalMs += sceneMs;
    gpuTiming_.postProcessTotalMs += postProcessMs;
    gpuTiming_.frameTotalMs += frameMs;
    if (gpuTiming_.samples == 1) {
        gpuTiming_.frameMinMs = frameMs;
        gpuTiming_.frameMaxMs = frameMs;
    } else {
        gpuTiming_.frameMinMs =
            std::min(gpuTiming_.frameMinMs, frameMs);
        gpuTiming_.frameMaxMs =
            std::max(gpuTiming_.frameMaxMs, frameMs);
    }
}

void AzureRenderApp::printGpuTimingSummary() const {
    if (!runOptions_.gpuTimingEnabled || gpuTiming_.samples == 0) {
        return;
    }
    const double count = static_cast<double>(gpuTiming_.samples);
    const double shadowAverage = gpuTiming_.shadowTotalMs / count;
    const double sceneAverage = gpuTiming_.sceneTotalMs / count;
    const double postAverage = gpuTiming_.postProcessTotalMs / count;
    const double frameAverage = gpuTiming_.frameTotalMs / count;
    const auto percentage = [frameAverage](const double value) {
        return frameAverage > 0.0
            ? value * 100.0 / frameAverage
            : 0.0;
    };
    std::cout
        << std::fixed << std::setprecision(3)
        << "GPU timing samples: " << gpuTiming_.samples << '\n'
        << "  Shadow: " << shadowAverage << " ms ("
        << percentage(shadowAverage) << "%)\n"
        << "  Main scene: " << sceneAverage << " ms ("
        << percentage(sceneAverage) << "%)\n"
        << "  Internal outline: " << postAverage << " ms ("
        << percentage(postAverage) << "%)\n"
        << "  Total render: " << frameAverage << " ms avg, "
        << gpuTiming_.frameMinMs << " ms min, "
        << gpuTiming_.frameMaxMs << " ms max\n";

    if (runOptions_.gpuTimingOutput.empty()) {
        return;
    }
    const std::filesystem::path outputPath =
        runOptions_.gpuTimingOutput;
    const std::filesystem::path parent = outputPath.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }
    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        throw std::runtime_error(
            "Failed to create GPU timing report: "
            + outputPath.string());
    }
    output
        << std::fixed << std::setprecision(6)
        << "{\n"
        << "  \"format\": \"Afterglow GPU timing v1\",\n"
        << "  \"gpu\": " << std::quoted(selectedGpuName_) << ",\n"
        << "  \"asset\": " << std::quoted(resolvedAssetPath_) << ",\n"
        << "  \"width\": " << swapchainExtent_.width << ",\n"
        << "  \"height\": " << swapchainExtent_.height << ",\n"
        << "  \"samples\": " << gpuTiming_.samples << ",\n"
        << "  \"timestampPeriodNanoseconds\": "
        << timestampPeriodNanoseconds_ << ",\n"
        << "  \"shadowAverageMs\": " << shadowAverage << ",\n"
        << "  \"mainSceneAverageMs\": " << sceneAverage << ",\n"
        << "  \"internalOutlineAverageMs\": " << postAverage << ",\n"
        << "  \"totalAverageMs\": " << frameAverage << ",\n"
        << "  \"totalMinMs\": " << gpuTiming_.frameMinMs << ",\n"
        << "  \"totalMaxMs\": " << gpuTiming_.frameMaxMs << "\n"
        << "}\n";
    if (!output) {
        throw std::runtime_error(
            "Failed to write GPU timing report: "
            + outputPath.string());
    }
    std::cout << "GPU timing report: " << outputPath.string() << '\n';
}

void AzureRenderApp::saveScreenshot(
    const VkDeviceMemory screenshotMemory,
    const std::uint32_t width,
    const std::uint32_t height,
    const std::string& requestedOutputPath) const {
    const std::size_t byteCount =
        static_cast<std::size_t>(width) * height * 4;
    void* mapped = nullptr;
    vkCheck(
        vkMapMemory(
            device_,
            screenshotMemory,
            0,
            static_cast<VkDeviceSize>(byteCount),
            0,
            &mapped),
        "vkMapMemory(screenshot)");
    std::vector<std::uint8_t> rgba(byteCount);
    std::memcpy(rgba.data(), mapped, byteCount);
    vkUnmapMemory(device_, screenshotMemory);

    const bool bgra =
        swapchainFormat_ == VK_FORMAT_B8G8R8A8_SRGB
        || swapchainFormat_ == VK_FORMAT_B8G8R8A8_UNORM;
    const bool rgbaFormat =
        swapchainFormat_ == VK_FORMAT_R8G8B8A8_SRGB
        || swapchainFormat_ == VK_FORMAT_R8G8B8A8_UNORM;
    if (!bgra && !rgbaFormat) {
        throw std::runtime_error(
            "Screenshot only supports 8-bit RGBA/BGRA swapchain formats");
    }
    if (bgra) {
        for (std::size_t pixel = 0; pixel < byteCount; pixel += 4) {
            std::swap(rgba[pixel], rgba[pixel + 2]);
        }
    }

    std::filesystem::path outputPath;
    if (requestedOutputPath.empty()) {
        const std::filesystem::path captureDirectory =
            AZURERENDER_CAPTURE_DIR;
        std::filesystem::create_directories(captureDirectory);
        const auto timestamp = std::chrono::duration_cast<
            std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        outputPath =
            captureDirectory
            / ("capture_" + std::to_string(timestamp) + ".png");
    } else {
        outputPath = requestedOutputPath;
    }
    if (stbi_write_png(
            outputPath.string().c_str(),
            static_cast<int>(width),
            static_cast<int>(height),
            4,
            rgba.data(),
            static_cast<int>(width * 4)) == 0) {
        throw std::runtime_error(
            "Failed to write screenshot: " + outputPath.string());
    }
    if (requestedOutputPath.empty()) {
        std::cout << "Screenshot saved: " << outputPath.string() << '\n';
    }
}
