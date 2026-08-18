#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "editor/EditorCameraController.hpp"
#include "editor/EditorContext.hpp"
#include "editor/EditorSession.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#include "extensions/ISceneRenderer.hpp"
#include "platform/GlfwFrontend.hpp"
#include "render/RenderContext.hpp"

#include <stb_easy_font.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

using namespace azurerender::internal;

void AzureRenderApp::drawFrame() {
    vkCheck(
        vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX),
        "vkWaitForFences");
    collectGpuTiming(currentFrame_);

    if (runOptions_.editorSession != nullptr) {
        if (runOptions_.editorSession->consumeAssetReloadRequest()) {
            vkCheck(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle(asset reload)");
            if (sceneRenderer_ != nullptr) {
                azurerender::RenderContext unloadContext;
                buildRenderContext(unloadContext);
                sceneRenderer_->onUnload(unloadContext);
                sceneRenderer_.reset();
            }
            createSceneRenderer();
            azurerender::RuntimeDiagnostics::instance().info(
                "editor", "Renderer resources reloaded");
        }
        std::string captureLabel;
        if (runOptions_.editorSession->consumeCaptureRequest(captureLabel)) {
            pendingScreenshotLabel_ = std::move(captureLabel);
            screenshotRequested_ = true;
        }
    }

    std::uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        framebufferResized_ = false;
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
    if (editorLayer_ != nullptr) {
        editorLayer_->setViewportImageIndex(imageIndex);
        editorLayer_->newFrame();
        editorLayer_->drawPanels();
        const azurerender::EditorViewportInput viewportInput =
            editorLayer_->consumeViewportInput();
        if (azurerender::EditorCameraController::apply(
                viewportInput,
                cameraPosition_,
                cameraTarget_)) {
            autoRotate_ = false;
        }
        if (viewportInput.pickRequested) {
            pendingPickX_ = viewportInput.pickX;
            pendingPickY_ = viewportInput.pickY;
        }
        std::uint32_t viewportWidth = 0;
        std::uint32_t viewportHeight = 0;
        if (editorLayer_->consumeViewportResizeRequest(
                viewportWidth, viewportHeight)) {
            requestedEditorViewportExtent_ = {
                viewportWidth,
                viewportHeight,
            };
            editorViewportResizeRequested_ = true;
        }
    }
    azurerender::SceneFrameData frameData;
    buildSceneFrameData(frameData);
    if (sceneRenderer_ != nullptr) {
        sceneRenderer_->updateFrame(frameData);
    }
    updateGizmoScreenData();
    if (pendingPickRequested_) {
        pendingPickRequested_ = false;
        pickPrimitive(pendingPickX_, pendingPickY_);
    }
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
            } else if (!pendingScreenshotLabel_.empty()) {
                const std::filesystem::path captureDirectory =
                    resourceLocator_.captureDirectory();
                std::filesystem::create_directories(captureDirectory);
                outputPath = (captureDirectory
                    / (pendingScreenshotLabel_ + ".png")).string();
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
                    azurerender::RuntimeDiagnostics::instance().info(
                        "capture",
                        "Capture progress: " + std::to_string(capturedFrames_)
                            + " / "
                            + std::to_string(runOptions_.captureFrameLimit));
                }
            }
            pendingScreenshotLabel_.clear();
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
    if (editorViewportResizeRequested_) {
        editorViewportResizeRequested_ = false;
        recreateEditorViewportResources();
    }

    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}


void AzureRenderApp::updateGizmoScreenData() {
    if (runOptions_.editorSession == nullptr) {
        return;
    }
    auto& editorContext = runOptions_.editorSession->context();
    editorContext.syncComponents();
    if (!ecsRenderableLogged_) {
        azurerender::RuntimeDiagnostics::instance().print(
            "ecs",
            "ECS visible renderables: "
                + std::to_string(editorContext.visibleRenderableCount()));
        ecsRenderableLogged_ = true;
    }
    editorContext.setGizmoScreen({});
    const azurerender::RendererSceneState* sceneState =
        sceneRenderer_ != nullptr ? sceneRenderer_->sceneState() : nullptr;
    if (sceneState == nullptr || sceneState->asset == nullptr
        || sceneState->modelMatrix == nullptr) {
        return;
    }
    const LoadedAsset& asset = *sceneState->asset;
    if (selectedPrimitiveIndex_ < 0
        || static_cast<std::size_t>(selectedPrimitiveIndex_)
            >= asset.primitives.size()) {
        return;
    }
    Matrix4 currentModel{};
    std::memcpy(currentModel.data(), sceneState->modelMatrix, sizeof(Matrix4));
    constexpr float kPi = 3.14159265358979323846F;
    const float aspect =
        static_cast<float>(renderExtent_.width)
        / static_cast<float>(renderExtent_.height);
    const Matrix4 view = lookAt(
        cameraPosition_, cameraTarget_, {0.0F, 1.0F, 0.0F});
    const Matrix4 projection =
        perspective(kPi / 3.0F, aspect, 0.1F, 100.0F);
    const Matrix4 viewProj = multiply(projection, view);
    const std::array<float, 3>& translation =
        editorContext.gizmoTranslation();
    const Vector3 primitiveCenter =
        transformPosition(currentModel,
            asset.primitives[selectedPrimitiveIndex_].center);
    const Vector3 gizmoCenter = {
        primitiveCenter[0] + translation[0],
        primitiveCenter[1] + translation[1],
        primitiveCenter[2] + translation[2],
    };
    const auto projectToScreen = [&viewProj](const Vector3& world) {
        const float clipX = viewProj[0] * world[0]
            + viewProj[4] * world[1]
            + viewProj[8] * world[2]
            + viewProj[12];
        const float clipY = viewProj[1] * world[0]
            + viewProj[5] * world[1]
            + viewProj[9] * world[2]
            + viewProj[13];
        const float clipW = viewProj[3] * world[0]
            + viewProj[7] * world[1]
            + viewProj[11] * world[2]
            + viewProj[15];
        if (std::abs(clipW) < 1.0e-6F) {
            return std::array<float, 2>{0.0F, 0.0F};
        }
        return std::array<float, 2>{clipX / clipW, clipY / clipW};
    };
    const std::array<float, 2> centerNdc = projectToScreen(gizmoCenter);
    if (centerNdc[0] < -1.2F || centerNdc[0] > 1.2F
        || centerNdc[1] < -1.2F || centerNdc[1] > 1.2F) {
        return;
    }
    constexpr float kAxisLen = 0.3F;
    const std::array<float, 2> endX =
        projectToScreen({gizmoCenter[0] + kAxisLen, gizmoCenter[1], gizmoCenter[2]});
    const std::array<float, 2> endY =
        projectToScreen({gizmoCenter[0], gizmoCenter[1] + kAxisLen, gizmoCenter[2]});
    const std::array<float, 2> endZ =
        projectToScreen({gizmoCenter[0], gizmoCenter[1], gizmoCenter[2] + kAxisLen});
    azurerender::EditorContext::GizmoScreenData data{};
    data.valid = true;
    data.centerX = (centerNdc[0] + 1.0F) * 0.5F;
    data.centerY = (1.0F - centerNdc[1]) * 0.5F;
    const auto normalize2D = [](float inX, float inY) {
        const float length = std::sqrt(inX * inX + inY * inY);
        if (length < 1.0e-6F) {
            return std::array<float, 2>{0.0F, 0.0F};
        }
        return std::array<float, 2>{inX / length, inY / length};
    };
    const std::array<float, 2> axisXScreen = normalize2D(
        (endX[0] - centerNdc[0]) * 0.5F,
        -(endX[1] - centerNdc[1]) * 0.5F);
    const std::array<float, 2> axisYScreen = normalize2D(
        (endY[0] - centerNdc[0]) * 0.5F,
        -(endY[1] - centerNdc[1]) * 0.5F);
    const std::array<float, 2> axisZScreen = normalize2D(
        (endZ[0] - centerNdc[0]) * 0.5F,
        -(endZ[1] - centerNdc[1]) * 0.5F);
    data.axisXScreenX = axisXScreen[0];
    data.axisXScreenY = axisXScreen[1];
    data.axisYScreenX = axisYScreen[0];
    data.axisYScreenY = axisYScreen[1];
    data.axisZScreenX = axisZScreen[0];
    data.axisZScreenY = axisZScreen[1];
    data.pixelToWorld = 0.005F;
    editorContext.setGizmoScreen(data);
}

void AzureRenderApp::pickPrimitive(
    const float viewportX,
    const float viewportY) {
    selectedPrimitiveIndex_ = -1;
    const azurerender::RendererSceneState* sceneState =
        sceneRenderer_ != nullptr ? sceneRenderer_->sceneState() : nullptr;
    if (sceneState == nullptr || sceneState->asset == nullptr
        || sceneState->modelMatrix == nullptr) {
        return;
    }
    const LoadedAsset& asset = *sceneState->asset;
    if (asset.indices.empty() || asset.vertices.empty()) {
        return;
    }
    Matrix4 currentModel{};
    std::memcpy(currentModel.data(), sceneState->modelMatrix, sizeof(Matrix4));
    // 从相机构建拾取射线(与 updateUniformBuffer 相同的 fov/aspect)。
    const float aspect =
        static_cast<float>(renderExtent_.width)
        / static_cast<float>(renderExtent_.height);
    const Vector3 direction = pickRayDirection(
        cameraPosition_,
        cameraTarget_,
        viewportX,
        viewportY,
        aspect);
    float bestDistance = std::numeric_limits<float>::max();
    for (std::size_t primitiveIndex = 0;
         primitiveIndex < asset.primitives.size();
         ++primitiveIndex) {
        const AssetPrimitive& primitive = asset.primitives[primitiveIndex];
        for (std::uint32_t offset = 0; offset + 2 < primitive.indexCount;
             offset += 3) {
            const std::uint32_t i0 =
                asset.indices[primitive.firstIndex + offset];
            const std::uint32_t i1 =
                asset.indices[primitive.firstIndex + offset + 1];
            const std::uint32_t i2 =
                asset.indices[primitive.firstIndex + offset + 2];
            const Vector3 v0 = transformPosition(
                currentModel, asset.vertices[i0].position);
            const Vector3 v1 = transformPosition(
                currentModel, asset.vertices[i1].position);
            const Vector3 v2 = transformPosition(
                currentModel, asset.vertices[i2].position);
            // Möller–Trumbore 求交(纯函数,可单测)。
            const float distance = rayTriangleDistance(
                cameraPosition_, direction, v0, v1, v2);
            if (distance > 0.0F && distance < bestDistance) {
                bestDistance = distance;
                selectedPrimitiveIndex_ =
                    static_cast<std::int32_t>(primitiveIndex);
            }
        }
    }
    if (selectedPrimitiveIndex_ >= 0
        && runOptions_.editorSession != nullptr) {
        azurerender::RuntimeDiagnostics::instance().print(
            "editor",
            "Pick: primitive "
                + std::to_string(selectedPrimitiveIndex_)
                + " / "
                + std::to_string(sceneState->primitiveCount));
    }
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

    std::ostringstream text;
    text << "AZURERENDER VULKAN RENDERER\n"
         << "GPU  : " << printable(selectedGpuName_, 46) << '\n'
         << "FRAME: " << swapchainExtent_.width << 'X'
         << swapchainExtent_.height
         << "  VIEW: " << kDiagnosticNames[renderSettings_.diagnosticView]
         << "  STYLE: " << (renderSettings_.stylizedLightingEnabled ? "ON" : "OFF")
         << "  OUTLINE: " << (renderSettings_.innerOutlineEnabled ? "ON" : "OFF")
         << '\n';
    if (sceneRenderer_ != nullptr) {
        sceneRenderer_->appendHudText(text);
    }
    text << "TOON : RAMP V1 / 10 CLASSES  MASK "
         << std::fixed << std::setprecision(2) << renderSettings_.styleMaskStrength
         << "  LEGACY THRESHOLD " << renderSettings_.diffuseBandThreshold << '\n';
    if (runOptions_.editorMode) {
        const azurerender::EditorContext& editorContext =
            runOptions_.editorSession->context();
        const azurerender::SceneDocument& editorScene = editorContext.scene();
        text << "EDITOR PREVIEW V1 | VIEWPORT: LIVE VULKAN\n"
             << "SCENE OUTLINER: ";
        if (editorScene.nodes.empty()) {
            text << "<EMPTY>";
        } else {
            for (std::size_t index = 0;
                 index < editorScene.nodes.size(); ++index) {
                if (index > 0) text << " | ";
                text << (index == editorContext.selectedNodeIndex()
                             ? "*" : " ")
                     << printable(editorScene.nodes[index].name, 20);
            }
        }
        text << "\nINSPECTOR: OUTLINE " << renderSettings_.outline.strength
             << "  EXPOSURE " << renderSettings_.grade.exposureEv
             << "  PRESET " << renderSettings_.showcasePreset << '\n'
             << "ASSET BROWSER: ";
        if (editorScene.resources.empty()) {
            text << "<EMPTY>";
        } else {
            for (std::size_t index = 0;
                 index < editorScene.resources.size(); ++index) {
                if (index > 0) text << " | ";
                text << printable(
                    editorScene.resources[index].path.string(), 38);
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

    azurerender::RenderContext sceneContext;
    buildRenderContext(sceneContext);
    sceneContext.currentFrame = static_cast<std::uint32_t>(currentFrame_);
    sceneContext.imageIndex = imageIndex;
    sceneContext.commandBuffer = commandBuffer;
    sceneContext.sceneFramebuffer = swapchainFramebuffers_[imageIndex];
    if (runOptions_.gpuTimingEnabled && !timestampQueryPools_.empty()) {
        sceneContext.timestampQueryPool =
            timestampQueryPools_[currentFrame_];
    }
    sceneContext.timestampQueryCount = kTimestampQueryCount;
    sceneContext.gpuTimingEnabled = runOptions_.gpuTimingEnabled;
    if (sceneRenderer_ != nullptr) {
        sceneRenderer_->recordScene(sceneContext);
    }

    VkRenderPassBeginInfo postProcessPassInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    postProcessPassInfo.renderPass = postProcessRenderPass_;
    postProcessPassInfo.framebuffer =
        postProcessFramebuffers_[imageIndex];

    postProcessPassInfo.renderArea.extent = renderExtent_;
    vkCmdBeginRenderPass(
        commandBuffer,
        &postProcessPassInfo,
        VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{};
    viewport.width = static_cast<float>(renderExtent_.width);
    viewport.height = static_cast<float>(renderExtent_.height);
    viewport.maxDepth = 1.0F;
    VkRect2D scissor{};
    scissor.extent = renderExtent_;
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
    if (editorUiEnabled_ && editorLayer_ != nullptr) {
        VkClearValue editorClear{};
        editorClear.color.float32[0] = 0.035F;
        editorClear.color.float32[1] = 0.040F;
        editorClear.color.float32[2] = 0.050F;
        editorClear.color.float32[3] = 1.0F;
        VkRenderPassBeginInfo editorPassInfo{
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        editorPassInfo.renderPass = editorUiRenderPass_;
        editorPassInfo.framebuffer = editorUiFramebuffers_[imageIndex];
        editorPassInfo.renderArea.extent = swapchainExtent_;
        editorPassInfo.clearValueCount = 1;
        editorPassInfo.pClearValues = &editorClear;
        vkCmdBeginRenderPass(
            commandBuffer, &editorPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport editorViewport{};
        editorViewport.width = static_cast<float>(swapchainExtent_.width);
        editorViewport.height = static_cast<float>(swapchainExtent_.height);
        editorViewport.maxDepth = 1.0F;
        VkRect2D editorScissor{};
        editorScissor.extent = swapchainExtent_;
        vkCmdSetViewport(commandBuffer, 0, 1, &editorViewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &editorScissor);
        editorLayer_->render(commandBuffer);
        vkCmdEndRenderPass(commandBuffer);
    }
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
