#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"
#include "platform/GlfwFrontend.hpp"

#include <stb_easy_font.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace azurerender::internal;

void AzureRenderApp::drawFrame() {
    vkCheck(
        vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX),
        "vkWaitForFences");
    collectGpuTiming(currentFrame_);

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR) {
        vkCheck(acquireResult, "vkAcquireNextImageKHR");
    }

    const bool captureSequenceFrame =
        fixedSimulation_
        && capturedFrames_ < runOptions_.captureFrameLimit;
    const bool captureThisFrame =
        screenshotRequested_ || captureSequenceFrame;
    screenshotRequested_ = false;
    VkBuffer screenshotBuffer = VK_NULL_HANDLE;
    VkDeviceMemory screenshotMemory = VK_NULL_HANDLE;
    if (captureThisFrame) {
        const VkDeviceSize screenshotSize =
            static_cast<VkDeviceSize>(swapchainExtent_.width)
            * swapchainExtent_.height
            * 4;
        createBuffer(
            screenshotSize,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            screenshotBuffer,
            screenshotMemory);
    }

    if (runOptions_.technicalSequence) {
        updateTechnicalSequenceState(capturedFrames_);
    }
    updateUniformBuffer(currentFrame_);
    updateHudBuffer(currentFrame_);
    vkCheck(vkResetFences(device_, 1, &inFlightFences_[currentFrame_]), "vkResetFences");
    vkCheck(vkResetCommandBuffer(commandBuffers_[currentFrame_], 0), "vkResetCommandBuffer");
    recordCommandBuffer(
        commandBuffers_[currentFrame_],
        imageIndex,
        screenshotBuffer);

    const VkSemaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
    const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkSemaphore signalSemaphores[] = {renderFinishedSemaphores_[imageIndex]};

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentFrame_];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]),
        "vkQueueSubmit");
    if (runOptions_.gpuTimingEnabled) {
        timestampQuerySubmitted_[currentFrame_] = true;
    }
    if (captureThisFrame) {
        vkCheck(
            vkWaitForFences(
                device_,
                1,
                &inFlightFences_[currentFrame_],
                VK_TRUE,
                UINT64_MAX),
            "vkWaitForFences(screenshot)");
        try {
            std::string outputPath;
            if (captureSequenceFrame) {
                std::ostringstream filename;
                filename
                    << "frame_" << std::setfill('0') << std::setw(6)
                    << capturedFrames_ << ".png";
                outputPath =
                    (std::filesystem::path(runOptions_.captureDirectory)
                     / filename.str()).string();
            }
            saveScreenshot(
                screenshotMemory,
                swapchainExtent_.width,
                swapchainExtent_.height,
                outputPath);
            if (captureSequenceFrame) {
                ++capturedFrames_;
                if (capturedFrames_ == 1
                    || capturedFrames_ == runOptions_.captureFrameLimit
                    || capturedFrames_ % runOptions_.captureFps == 0) {
                    std::cout
                        << "Capture progress: " << capturedFrames_ << " / "
                        << runOptions_.captureFrameLimit << '\n';
                }
            }
        } catch (...) {
            vkDestroyBuffer(device_, screenshotBuffer, nullptr);
            vkFreeMemory(device_, screenshotMemory, nullptr);
            throw;
        }
        vkDestroyBuffer(device_, screenshotBuffer, nullptr);
        vkFreeMemory(device_, screenshotMemory, nullptr);
    }

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR
        || presentResult == VK_SUBOPTIMAL_KHR
        || framebufferResized_) {
        framebufferResized_ = false;
        recreateSwapchain();
    } else if (presentResult != VK_SUCCESS) {
        vkCheck(presentResult, "vkQueuePresentKHR");
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}

void AzureRenderApp::updateUniformBuffer(const std::size_t frameIndex) {
    const double currentTime = frontend_->timeSeconds();
    const float deltaSeconds = fixedSimulation_
        ? (fixedSimulationStarted_ ? fixedDeltaSeconds_ : 0.0F)
        : static_cast<float>(
              std::max(currentTime - lastRotationTime_, 0.0));
    fixedSimulationStarted_ = true;
    lastRotationTime_ = currentTime;
    if (autoRotate_) {
        rotationAngle_ += deltaSeconds * rotationSpeed_;
    }
    if (!asset_.animations.empty()) {
        if (animationPlaying_) {
            animationTime_ += deltaSeconds;
        }
        sampleAnimation(
            asset_,
            animationIndex_,
            animationTime_,
            asset_.jointMatrices);
        const std::size_t jointBytes =
            sizeof(asset_.jointMatrices.front())
            * asset_.jointMatrices.size();
        std::memcpy(
            jointBufferMapped_[frameIndex],
            asset_.jointMatrices.data(),
            jointBytes);
    }

    const Vector3 center = {
        (asset_.boundsMin[0] + asset_.boundsMax[0]) * 0.5F,
        (asset_.boundsMin[1] + asset_.boundsMax[1]) * 0.5F,
        (asset_.boundsMin[2] + asset_.boundsMax[2]) * 0.5F,
    };
    const float largestExtent = std::max({
        asset_.boundsMax[0] - asset_.boundsMin[0],
        asset_.boundsMax[1] - asset_.boundsMin[1],
        asset_.boundsMax[2] - asset_.boundsMin[2],
    });
    const float fitScale = largestExtent > 0.0F ? 2.5F / largestExtent : 1.0F;
    const Matrix4 model = multiply(
        rotationY(rotationAngle_),
        multiply(
            uniformScale(fitScale),
            translation(-center[0], -center[1], -center[2])));
    currentModel_ = model;
    const Matrix4 view = lookAt(
        cameraPosition_,
        cameraTarget_,
        {0.0F, 1.0F, 0.0F});
    const float aspect =
        static_cast<float>(swapchainExtent_.width)
        / static_cast<float>(swapchainExtent_.height);
    constexpr float kPi = 3.14159265358979323846F;
    const Matrix4 projection = perspective(kPi / 3.0F, aspect, 0.1F, 100.0F);
    const Vector3 lightDirection = normalize({0.48F, 0.82F, 0.32F});
    const Vector3 lightTarget = {0.0F, -0.10F, 0.0F};
    const Vector3 lightPosition = {
        lightTarget[0] + lightDirection[0] * 4.5F,
        lightTarget[1] + lightDirection[1] * 4.5F,
        lightTarget[2] + lightDirection[2] * 4.5F,
    };
    const Matrix4 lightView = lookAt(
        lightPosition,
        lightTarget,
        {0.0F, 1.0F, 0.0F});
    const Matrix4 lightProjection = orthographic(
        -1.90F,
        1.90F,
        -1.90F,
        1.90F,
        0.10F,
        8.0F);

    UniformBufferObject uniform{};
    uniform.model = model;
    uniform.modelViewProjection = multiply(projection, multiply(view, model));
    uniform.lightModelViewProjection =
        multiply(lightProjection, multiply(lightView, model));
    uniform.cameraPosition = {
        cameraPosition_[0],
        cameraPosition_[1],
        cameraPosition_[2],
        1.0F,
    };
    uniform.renderingParameters = {
        largestExtent * 0.004F,
        renderSettings_.stylizedLightingEnabled
            ? renderSettings_.styleMaskStrength
            : 0.0F,
        renderSettings_.stylizedLightingEnabled
            ? renderSettings_.diffuseBandThreshold
            : -1.0F,
        0.12F,
    };
    constexpr std::array<std::array<float, 4>, 5> kShowcasePresets = {{
        {0.0F, 1.00F, 0.13F, 0.12F},
        {1.0F, 0.92F, 0.16F, 0.16F},
        {2.0F, 0.95F, 0.08F, 0.05F},
        {3.0F, 0.48F, 0.04F, 0.85F},
        {4.0F, 0.18F, 0.02F, 0.08F},
    }};
    uniform.showcaseParameters =
        kShowcasePresets[std::min<std::size_t>(
            renderSettings_.showcasePreset,
            kShowcasePresets.size() - 1)];
    uniform.qaParameters = {
        static_cast<float>(qaIsolationMode_),
        static_cast<float>(qaEffectMode_),
        qaEffectEnabled_ ? 1.0F : 0.0F,
        qaHarnessEnabled_ ? 1.0F : 0.0F,
    };
    Vector3 faceLight = {0.0F, 0.0F, -1.0F};
    bool hasFaceSdf = false;
    if (faceSdfHeadNode_.has_value()
        && *faceSdfHeadNode_ < asset_.nodeWorldMatrices.size()) {
        const float cosine = std::cos(rotationAngle_);
        const float sine = std::sin(rotationAngle_);
        const Vector3 objectLight = normalize({
            cosine * lightDirection[0] - sine * lightDirection[2],
            lightDirection[1],
            sine * lightDirection[0] + cosine * lightDirection[2],
        });
        const auto& head = asset_.nodeWorldMatrices[*faceSdfHeadNode_];
        const Vector3 headX = normalize({head[0], head[1], head[2]});
        const Vector3 headY = normalize({head[4], head[5], head[6]});
        const Vector3 headZ = normalize({head[8], head[9], head[10]});
        faceLight = normalize({
            dot(objectLight, headX),
            dot(objectLight, headY),
            dot(objectLight, headZ),
        });
        hasFaceSdf = true;
    }
    uniform.faceLightDirection = {
        faceLight[0], faceLight[1], faceLight[2], hasFaceSdf ? 1.0F : 0.0F,
    };
    uniform.faceSdfParameters = {
        renderSettings_.faceSdf.enabled ? 1.0F : 0.0F,
        renderSettings_.faceSdf.threshold,
        renderSettings_.faceSdf.softness,
        renderSettings_.faceSdf.mirrorHorizontal ? 1.0F : 0.0F,
    };
    uniform.faceSdfShadowColor = renderSettings_.faceSdf.shadowColor;
    std::memcpy(
        uniformBufferMapped_[frameIndex],
        &uniform,
        sizeof(uniform));
}

void AzureRenderApp::updateHudBuffer(const std::size_t frameIndex) {
    hudVertexCounts_[frameIndex] = 0;
    if (!hudEnabled_ && !runOptions_.technicalSequence) {
        return;
    }

    struct EasyFontVertex {
        float x;
        float y;
        float z;
        std::array<std::uint8_t, 4> color;
    };
    static_assert(sizeof(EasyFontVertex) == 16);
    auto* destination =
        static_cast<HudVertex*>(hudVertexBufferMapped_[frameIndex]);
    std::uint32_t vertexCount = 0;
    const float width = static_cast<float>(swapchainExtent_.width);
    const float height = static_cast<float>(swapchainExtent_.height);
    const auto toNdc = [width, height](const float x, const float y) {
        return std::array<float, 2>{
            x / width * 2.0F - 1.0F,
            y / height * 2.0F - 1.0F,
        };
    };
    const auto appendRectangle =
        [&](const float x0,
            const float y0,
            const float x1,
            const float y1,
            const std::array<std::uint8_t, 4>& color) {
            const std::array<std::array<float, 2>, 4> corners = {
                toNdc(x0, y0),
                toNdc(x1, y0),
                toNdc(x1, y1),
                toNdc(x0, y1),
            };
            constexpr std::array<std::uint32_t, 6> indices = {
                0, 1, 2, 0, 2, 3,
            };
            for (const std::uint32_t corner : indices) {
                if (vertexCount >= kMaxHudVertices) {
                    return;
                }
                destination[vertexCount++] = {
                    corners[corner],
                    color,
                };
            }
        };
    const auto appendText =
        [&](std::string value,
            const float x,
            const float y,
            const float scale,
            const std::array<std::uint8_t, 4>& color) {
            std::array<float, 32768> easyFontBuffer{};
            std::array<unsigned char, 4> easyColor = color;
            stb_easy_font_spacing(-0.25F);
            const int quadCount = stb_easy_font_print(
                0.0F,
                0.0F,
                value.data(),
                easyColor.data(),
                easyFontBuffer.data(),
                static_cast<int>(sizeof(easyFontBuffer)));
            const auto* source = reinterpret_cast<
                const EasyFontVertex*>(easyFontBuffer.data());
            constexpr std::array<std::uint32_t, 6> indices = {
                0, 1, 2, 0, 2, 3,
            };
            for (int quad = 0; quad < quadCount; ++quad) {
                for (const std::uint32_t corner : indices) {
                    if (vertexCount >= kMaxHudVertices) {
                        return;
                    }
                    const EasyFontVertex& sourceVertex =
                        source[quad * 4 + corner];
                    destination[vertexCount++] = {
                        toNdc(
                            x + sourceVertex.x * scale,
                            y + sourceVertex.y * scale),
                        sourceVertex.color,
                    };
                }
            }
        };

    std::uint64_t localChapterFrame = 0;
    std::uint64_t fadeFrames = 0;
    bool showHud = hudEnabled_;
    if (runOptions_.technicalSequence) {
        const std::uint64_t chapterFrames =
            runOptions_.captureFrameLimit / 5;
        localChapterFrame = capturedFrames_ % chapterFrames;
        fadeFrames = std::min<std::uint64_t>(
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
        float fadeOpacity = 0.0F;
        if (localChapterFrame < fadeFrames) {
            fadeOpacity =
                1.0F
                - static_cast<float>(localChapterFrame)
                    / static_cast<float>(fadeFrames);
        } else if (localChapterFrame
                   >= chapterFrames - fadeFrames) {
            fadeOpacity =
                static_cast<float>(
                    localChapterFrame
                    - (chapterFrames - fadeFrames)
                    + 1)
                / static_cast<float>(fadeFrames);
        }
        if (fadeOpacity > 0.0F) {
            appendRectangle(
                0.0F,
                0.0F,
                width,
                height,
                {
                    2,
                    5,
                    10,
                    static_cast<std::uint8_t>(
                        std::clamp(fadeOpacity, 0.0F, 1.0F)
                        * 255.0F),
                });
        }

        if (localChapterFrame < titleFrames) {
            constexpr std::array<const char*, 5> kTitles = {
                "BEAUTY RENDER",
                "WORLD NORMAL",
                "INTERNAL OUTLINE",
                "SHADOW MAP",
                "BEAUTY + GPU HUD",
            };
            constexpr std::array<const char*, 5> kSubtitles = {
                "STYLIZED FORWARD OUTPUT",
                "GEOMETRIC NORMAL ATTACHMENT",
                "DEPTH + NORMAL EDGE RESPONSE",
                "2048 X 2048 LIGHT SPACE DEPTH",
                "LIVE VULKAN PASS TIMING",
            };
            const std::size_t chapter = std::min<std::size_t>(
                technicalSequenceChapter_,
                kTitles.size() - 1);
            const std::uint64_t titleFadeFrames =
                std::max<std::uint64_t>(
                    1,
                    std::min<std::uint64_t>(
                        runOptions_.captureFps / 4,
                        titleFrames / 4));
            float titleOpacity = 1.0F;
            if (localChapterFrame < titleFadeFrames) {
                titleOpacity =
                    static_cast<float>(localChapterFrame)
                    / static_cast<float>(titleFadeFrames);
            } else if (localChapterFrame
                       >= titleFrames - titleFadeFrames) {
                titleOpacity =
                    static_cast<float>(
                        titleFrames - localChapterFrame)
                    / static_cast<float>(titleFadeFrames);
            }
            const float titleScale =
                std::clamp(height / 720.0F, 1.0F, 1.5F) * 1.65F;
            const float subtitleScale = titleScale * 0.58F;
            std::string title = kTitles[chapter];
            std::string subtitle = kSubtitles[chapter];
            const float titleWidth =
                static_cast<float>(stb_easy_font_width(title.data()))
                * titleScale;
            const float subtitleWidth =
                static_cast<float>(stb_easy_font_width(subtitle.data()))
                * subtitleScale;
            const std::uint8_t alpha = static_cast<std::uint8_t>(
                std::clamp(titleOpacity, 0.0F, 1.0F) * 255.0F);
            const float titleY = height * 0.43F;
            appendText(
                title,
                (width - titleWidth) * 0.5F,
                titleY,
                titleScale,
                {226, 246, 250, alpha});
            appendText(
                subtitle,
                (width - subtitleWidth) * 0.5F,
                titleY + 28.0F * titleScale,
                subtitleScale,
                {83, 216, 238, alpha});
        }
        showHud = hudEnabled_
            && localChapterFrame >= fadeFrames;
    }
    if (!showHud) {
        hudVertexCounts_[frameIndex] = vertexCount;
        return;
    }

    const auto printable = [](std::string value, const std::size_t limit) {
        for (char& character : value) {
            const unsigned char code =
                static_cast<unsigned char>(character);
            if (code < 32 || code > 126) {
                character = '?';
            }
        }
        if (value.size() > limit) {
            value.resize(limit);
        }
        return value;
    };
    constexpr std::array<const char*, 5> kDiagnosticNames = {
        "BEAUTY",
        "WORLD NORMAL",
        "INTERNAL OUTLINE",
        "SHADOW MAP",
        "DEPTH",
    };
    std::string animationName = "NONE";
    float animationDuration = 0.0F;
    float animationPlayhead = 0.0F;
    if (!asset_.animations.empty()) {
        const AssetAnimation& animation =
            asset_.animations[animationIndex_];
        animationName = printable(
            animation.name.empty() ? "UNNAMED" : animation.name,
            30);
        animationDuration =
            std::max(animation.endTime - animation.startTime, 0.0F);
        if (animationDuration > 1.0e-8F) {
            animationPlayhead = std::fmod(
                std::max(animationTime_, 0.0F),
                animationDuration);
        }
    }

    std::ostringstream text;
    std::array<std::size_t, 10> materialClassCounts{};
    const AssetMaterial* faceProfile = nullptr;
    const AssetMaterial* hairProfile = nullptr;
    for (const AssetMaterial& material : asset_.materials) {
        const std::size_t classIndex = static_cast<std::size_t>(
            material.materialClass);
        if (classIndex < materialClassCounts.size()) {
            ++materialClassCounts[classIndex];
        }
        if (material.materialClass == AssetMaterialClass::Face) {
            faceProfile = &material;
        } else if (material.materialClass == AssetMaterialClass::Hair) {
            hairProfile = &material;
        }
    }
    text << "AZURERENDER VULKAN RENDERER\n"
         << "GPU  : " << printable(selectedGpuName_, 46) << '\n'
         << "FRAME: " << swapchainExtent_.width << 'X'
         << swapchainExtent_.height
         << "  VIEW: " << kDiagnosticNames[renderSettings_.diagnosticView]
         << "  STYLE: " << (renderSettings_.stylizedLightingEnabled ? "ON" : "OFF")
         << "  OUTLINE: " << (renderSettings_.innerOutlineEnabled ? "ON" : "OFF")
         << '\n'
         << "ANIM : " << animationName << "  "
         << std::fixed << std::setprecision(2)
         << animationPlayhead << '/' << animationDuration << " S  "
         << (animationPlaying_ ? "PLAYING" : "PAUSED") << '\n';
    text << "MAT V1: SKIN " << materialClassCounts[1]
         << " FACE " << materialClassCounts[2]
         << " HAIR " << materialClassCounts[3]
         << " FABRIC " << materialClassCounts[4]
         << " METAL " << materialClassCounts[5]
         << " EYE " << materialClassCounts[6]
         << " OVERLAY " << materialClassCounts[7] << '\n'
         << "TOON : RAMP V1 / 10 CLASSES  MASK "
         << std::fixed << std::setprecision(2) << renderSettings_.styleMaskStrength
         << "  LEGACY THRESHOLD " << renderSettings_.diffuseBandThreshold << '\n';
    if (faceProfile != nullptr && hairProfile != nullptr) {
        text << "MAT PARAM: FACE T" << faceProfile->styleParameters[0]
             << " S" << faceProfile->styleParameters[2]
             << " R" << faceProfile->styleParameters[3]
             << " | HAIR T" << hairProfile->styleParameters[0]
             << " S" << hairProfile->styleParameters[2]
             << " R" << hairProfile->styleParameters[3] << '\n';
    }
    if (runOptions_.editorMode) {
        text << "EDITOR PREVIEW V1 | VIEWPORT: LIVE VULKAN\n"
             << "SCENE OUTLINER: ";
        if (runOptions_.editorNodeNames.empty()) {
            text << "<EMPTY>";
        } else {
            for (std::size_t index = 0;
                 index < runOptions_.editorNodeNames.size(); ++index) {
                if (index > 0) text << " | ";
                text << (index == editorSelectedNode_ ? "*" : " ")
                     << printable(runOptions_.editorNodeNames[index], 20);
            }
        }
        text << "\nINSPECTOR: OUTLINE " << renderSettings_.outline.strength
             << "  EXPOSURE " << renderSettings_.grade.exposureEv
             << "  PRESET " << renderSettings_.showcasePreset << '\n'
             << "ASSET BROWSER: ";
        if (runOptions_.editorResourcePaths.empty()) {
            text << "<EMPTY>";
        } else {
            for (std::size_t index = 0;
                 index < runOptions_.editorResourcePaths.size(); ++index) {
                if (index > 0) text << " | ";
                text << printable(runOptions_.editorResourcePaths[index], 38);
            }
        }
        text << "\nCONSOLE: [/] OUTLINE  -/= EXPOSURE  F1-F3 LIGHT  CLOSE=SAVES\n";
    }
    if (gpuTiming_.samples > 0) {
        const double count = static_cast<double>(gpuTiming_.samples);
        text << "GPU MS: SHADOW "
             << gpuTiming_.shadowTotalMs / count
             << "  MAIN " << gpuTiming_.sceneTotalMs / count
             << "  OUTLINE " << gpuTiming_.postProcessTotalMs / count
             << "  TOTAL " << gpuTiming_.frameTotalMs / count;
    } else if (runOptions_.gpuTimingEnabled) {
        text << "GPU MS: WARMING UP";
    } else {
        text << "GPU MS: ENABLE WITH --HUD OR --GPU-TIMING";
    }
    std::string textValue = text.str();

    const int textWidth = stb_easy_font_width(textValue.data());
    const int textHeight = stb_easy_font_height(textValue.data());

    const float scale = std::clamp(height / 720.0F, 1.0F, 1.5F);
    const float panelX = 18.0F;
    const float panelY = 18.0F;
    const float textX = panelX + 18.0F;
    const float textY = panelY + 16.0F;
    const float panelWidth =
        std::min(
            static_cast<float>(textWidth) * scale + 36.0F,
            width - panelX * 2.0F);
    const float panelHeight =
        static_cast<float>(textHeight) * scale + 32.0F;
    appendRectangle(
        panelX,
        panelY,
        panelX + panelWidth,
        panelY + panelHeight,
        {7, 15, 27, 224});
    appendRectangle(
        panelX,
        panelY,
        panelX + 5.0F,
        panelY + panelHeight,
        {54, 207, 232, 255});
    appendText(
        textValue,
        textX,
        textY,
        scale,
        {218, 241, 248, 255});
    hudVertexCounts_[frameIndex] = vertexCount;
}



void AzureRenderApp::recordCommandBuffer(
    const VkCommandBuffer commandBuffer,
    const std::uint32_t imageIndex,
    const VkBuffer screenshotBuffer) {
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");
    if (runOptions_.gpuTimingEnabled) {
        vkCmdResetQueryPool(
            commandBuffer,
            timestampQueryPools_[currentFrame_],
            0,
            kTimestampQueryCount);
        vkCmdWriteTimestamp(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            timestampQueryPools_[currentFrame_],
            0);
    }

    VkClearValue shadowClear{};
    shadowClear.depthStencil = {1.0F, 0};
    VkRenderPassBeginInfo shadowPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    shadowPassInfo.renderPass = shadowRenderPass_;
    shadowPassInfo.framebuffer = shadowFramebuffer_;
    shadowPassInfo.renderArea.extent = {
        kShadowMapSize,
        kShadowMapSize,
    };
    shadowPassInfo.clearValueCount = 1;
    shadowPassInfo.pClearValues = &shadowClear;
    vkCmdBeginRenderPass(
        commandBuffer,
        &shadowPassInfo,
        VK_SUBPASS_CONTENTS_INLINE);

    VkViewport shadowViewport{};
    shadowViewport.width = static_cast<float>(kShadowMapSize);
    shadowViewport.height = static_cast<float>(kShadowMapSize);
    shadowViewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer, 0, 1, &shadowViewport);
    VkRect2D shadowScissor{};
    shadowScissor.extent = {kShadowMapSize, kShadowMapSize};
    vkCmdSetScissor(commandBuffer, 0, 1, &shadowScissor);
    const VkDeviceSize shadowOffsets[] = {0};
    vkCmdBindVertexBuffers(
        commandBuffer,
        0,
        1,
        &vertexBuffer_,
        shadowOffsets);
    vkCmdBindIndexBuffer(
        commandBuffer,
        indexBuffer_,
        0,
        VK_INDEX_TYPE_UINT32);
    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        shadowPipeline_);
    for (const AssetPrimitive& primitive : asset_.primitives) {
        const AssetMaterial& material =
            asset_.materials[primitive.materialIndex];
        if (material.showcasePlatform > 0.5F
            || material.materialClass == AssetMaterialClass::Overlay) {
            continue;
        }
        const std::size_t descriptorIndex =
            currentFrame_ * asset_.materials.size()
            + primitive.materialIndex;
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[descriptorIndex],
            0,
            nullptr);
        const MaterialPushConstants materialConstants{
            material.alphaCutoff,
            static_cast<std::uint32_t>(material.alphaMode),
            material.emissiveStrength,
            material.showcasePlatform,
            material.aoColor,
            material.lamShadowColor,
            material.matcapColor,
            material.hairParameters,
            material.styleParameters,
            material.featureParameters,
            static_cast<std::uint32_t>(material.materialClass),
            material.materialFeatures,
            material.materialProfileVersion,
            0,
        };
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(materialConstants),
            &materialConstants);
        vkCmdDrawIndexed(
            commandBuffer,
            primitive.indexCount,
            1,
            primitive.firstIndex,
            0,
            0);
    }
    vkCmdEndRenderPass(commandBuffer);
    if (runOptions_.gpuTimingEnabled) {
        vkCmdWriteTimestamp(
            commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            timestampQueryPools_[currentFrame_],
            1);
    }

    std::array<VkClearValue, 3> clearValues{};
    clearValues[0].color.float32[0] = 0.035F;
    clearValues[0].color.float32[1] = 0.055F;
    clearValues[0].color.float32[2] = 0.075F;
    clearValues[0].color.float32[3] = 1.0F;
    clearValues[1].depthStencil = {1.0F, 0};
    clearValues[2].color.float32[0] = 0.5F;
    clearValues[2].color.float32[1] = 0.5F;
    clearValues[2].color.float32[2] = 1.0F;
    clearValues[2].color.float32[3] = 0.0F;
    VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = swapchainFramebuffers_[imageIndex];
    renderPassInfo.renderArea.extent = swapchainExtent_;
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchainExtent_.width);
    viewport.height = static_cast<float>(swapchainExtent_.height);
    viewport.maxDepth = 1.0F;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = swapchainExtent_;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);


    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        backgroundPipeline_);
    const std::size_t backgroundDescriptorIndex =
        currentFrame_ * asset_.materials.size();
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout_,
        0,
        1,
        &descriptorSets_[backgroundDescriptorIndex],
        0,
        nullptr);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    const VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer_, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    if (renderSettings_.silhouetteOutlineEnabled) {
        vkCmdBindPipeline(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            outlinePipeline_);
        for (const AssetPrimitive& primitive : asset_.primitives) {
            if (asset_.materials[primitive.materialIndex].alphaMode
                == AssetAlphaMode::Blend) {
                continue;
            }
            const std::size_t descriptorIndex =
                currentFrame_ * asset_.materials.size()
                + primitive.materialIndex;
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipelineLayout_,
                0,
                1,
                &descriptorSets_[descriptorIndex],
                0,
                nullptr);
            vkCmdDrawIndexed(
                commandBuffer,
                primitive.indexCount,
                1,
                primitive.firstIndex,
                0,
                0);
        }
    }
    const auto drawPrimitive = [&](const AssetPrimitive& primitive) {
        const AssetMaterial& material = asset_.materials[primitive.materialIndex];
        const bool blend = material.alphaMode == AssetAlphaMode::Blend;
        const VkPipeline pipeline = blend
            ? (material.doubleSided ? blendDoubleSidedPipeline_ : blendPipeline_)
            : (material.doubleSided ? opaqueDoubleSidedPipeline_ : opaquePipeline_);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        const std::size_t descriptorIndex =
            currentFrame_ * asset_.materials.size() + primitive.materialIndex;
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            pipelineLayout_,
            0,
            1,
            &descriptorSets_[descriptorIndex],
            0,
            nullptr);
        const MaterialPushConstants materialConstants{
            material.alphaCutoff,
            static_cast<std::uint32_t>(material.alphaMode),
            material.emissiveStrength,
            material.showcasePlatform,
            material.aoColor,
            material.lamShadowColor,
            material.matcapColor,
            material.hairParameters,
            material.styleParameters,
            material.featureParameters,
            static_cast<std::uint32_t>(material.materialClass),
            material.materialFeatures,
            material.materialProfileVersion,
            0,
        };
        vkCmdPushConstants(
            commandBuffer,
            pipelineLayout_,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(materialConstants),
            &materialConstants);
        vkCmdDrawIndexed(
            commandBuffer,
            primitive.indexCount,
            1,
            primitive.firstIndex,
            0,
            0);
    };
    for (const AssetPrimitive& primitive : asset_.primitives) {
        if (asset_.materials[primitive.materialIndex].alphaMode != AssetAlphaMode::Blend) {
            drawPrimitive(primitive);
        }
    }
    std::vector<const AssetPrimitive*> transparentPrimitives;
    for (const AssetPrimitive& primitive : asset_.primitives) {
        if (asset_.materials[primitive.materialIndex].alphaMode
            == AssetAlphaMode::Blend) {
            transparentPrimitives.push_back(&primitive);
        }
    }
    const Vector3 cameraForward = normalize({
        -cameraPosition_[0],
        -cameraPosition_[1],
        -cameraPosition_[2],
    });
    const auto viewDepth = [&](const AssetPrimitive& primitive) {
        const Vector3 worldCenter = transformPosition(
            currentModel_,
            primitive.center);
        const Vector3 cameraOffset = subtract(worldCenter, cameraPosition_);
        return dot(cameraOffset, cameraForward);
    };
    std::stable_sort(
        transparentPrimitives.begin(),
        transparentPrimitives.end(),
        [&](const AssetPrimitive* left, const AssetPrimitive* right) {
            return viewDepth(*left) > viewDepth(*right);
        });
    for (const AssetPrimitive* primitive : transparentPrimitives) {
        drawPrimitive(*primitive);
    }
    vkCmdEndRenderPass(commandBuffer);
    if (runOptions_.gpuTimingEnabled) {
        vkCmdWriteTimestamp(
            commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            timestampQueryPools_[currentFrame_],
            2);
    }

    VkRenderPassBeginInfo postProcessPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    postProcessPassInfo.renderPass = postProcessRenderPass_;
    postProcessPassInfo.framebuffer =
        postProcessFramebuffers_[imageIndex];

    postProcessPassInfo.renderArea.extent = swapchainExtent_;
    vkCmdBeginRenderPass(
        commandBuffer,
        &postProcessPassInfo,
        VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdBindPipeline(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        innerOutlinePipeline_);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        postProcessPipelineLayout_,
        0,
        1,
        &postProcessDescriptorSets_[imageIndex],
        0,
        nullptr);
    const PostProcessPushConstants postProcessConstants{
        renderSettings_.diagnosticView == 2
            ? 1.0F
            : (renderSettings_.innerOutlineEnabled
                ? renderSettings_.outline.strength : 0.0F),
        renderSettings_.outline.depthThreshold,
        renderSettings_.outline.normalThreshold,
        static_cast<float>(renderSettings_.diagnosticView),
        renderSettings_.grade.exposureEv,
        renderSettings_.grade.toneMappingEnabled ? 1.0F : 0.0F,
        renderSettings_.bloom.enabled
            && !(qaEffectName_ == "bloom" && !qaEffectEnabled_)
            ? renderSettings_.bloom.strength : 0.0F,
        qaEffectName_ == "bloom" && qaEffectStateName_ == "isolation"
            ? 1.0F : 0.0F,
        {
            renderSettings_.outline.color[0],
            renderSettings_.outline.color[1],
            renderSettings_.outline.color[2],
            1.0F,
        },
        {
            renderSettings_.grade.saturation,
            renderSettings_.grade.contrast,
            renderSettings_.bloom.threshold,
            0.0F,
        },
        {
            renderSettings_.grade.tint[0],
            renderSettings_.grade.tint[1],
            renderSettings_.grade.tint[2],
            0.0F,
        },
    };
    vkCmdPushConstants(
        commandBuffer,
        postProcessPipelineLayout_,
        VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(postProcessConstants),
        &postProcessConstants);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    if ((hudEnabled_ || runOptions_.technicalSequence) &&
        hudVertexCounts_[currentFrame_] > 0) {
        vkCmdBindPipeline(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            hudPipeline_);
        const VkDeviceSize hudOffset = 0;
        vkCmdBindVertexBuffers(
            commandBuffer,
            0,
            1,
            &hudVertexBuffers_[currentFrame_],
            &hudOffset);
        vkCmdDraw(
            commandBuffer,
            hudVertexCounts_[currentFrame_],
            1,
            0,
            0);
    }
    vkCmdEndRenderPass(commandBuffer);
    if (runOptions_.gpuTimingEnabled) {
        vkCmdWriteTimestamp(
            commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            timestampQueryPools_[currentFrame_],
            3);
    }

    if (screenshotBuffer != VK_NULL_HANDLE) {
        VkImageMemoryBarrier toTransfer{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toTransfer.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = swapchainImages_[imageIndex];
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {
            swapchainExtent_.width,
            swapchainExtent_.height,
            1,
        };
        vkCmdCopyImageToBuffer(
            commandBuffer,
            swapchainImages_[imageIndex],
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            screenshotBuffer,
            1,
            &copy);

        VkImageMemoryBarrier toPresent = toTransfer;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        toPresent.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toPresent);
    }

    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}
