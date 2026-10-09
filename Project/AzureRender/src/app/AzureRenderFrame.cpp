#include "assets/AssetDeformation.hpp"
#include "AzureRenderApp.hpp"
#include "AzureRenderInternal.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "render/CameraProjection.hpp"
#if AZURE_WITH_EDITOR
#include "editor/EditorCameraController.hpp"
#include "editor/viewport/EditorCameraService.hpp"
#endif
#if AZURE_WITH_EDITOR
#include "editor/EditorContext.hpp"
#endif
#if AZURE_WITH_EDITOR
#include "editor/EditorSession.hpp"
#endif
#if AZURE_WITH_EDITOR
#include "editor/ImGuiEditorLayer.hpp"
#endif
#include "extensions/ISceneRenderer.hpp"
#include "platform/GlfwFrontend.hpp"
#include "render/RenderContext.hpp"
#include "render/RenderGraph.hpp"
#include "render/RenderFrameSnapshot.hpp"
#include "render/FrameTaskScheduler.hpp"

#include <stb_easy_font.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
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
    const auto frameStart = std::chrono::steady_clock::now();
    const double previousWait=submissionCounters_.frameSlotWaitMilliseconds+submissionCounters_.acquireMilliseconds
        +submissionCounters_.submitMilliseconds+submissionCounters_.presentMilliseconds;
    ++gameplayFrame_;
    if (runOptions_.gpuTimingEnabled) ++submissionCounters_.frameAttempts;
    vkCheck(
        vkWaitForFences(device_, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX),
        "vkWaitForFences");
    graphicsCompleted_=std::max(graphicsCompleted_,graphicsFrameSubmissions_[currentFrame_]);
#if AZURE_WITH_EDITOR
    pollDevtools();
    if(editorLayer_)editorLayer_->completePreviewTextures(graphicsCompleted_);
#endif
    const auto fenceEnd = std::chrono::steady_clock::now();
    if (runOptions_.gpuTimingEnabled)
        submissionCounters_.frameSlotWaitMilliseconds += std::chrono::duration<double, std::milli>(fenceEnd - frameStart).count();
    synchronizeEditorRuntime();
    synchronizeGameUi();
    engineSettings_->applyPending();effectiveRenderSettings_=azurerender::resolveRenderSettings(*engineSettings_,renderSettings_);
    if(settingsRevision_!=engineSettings_->revision()) {
        settingsRevision_=engineSettings_->revision();
        if(engineSettings_->get("diagnostics.verbose").get<bool>())
            azurerender::RuntimeDiagnostics::instance().info("settings","Applied setting layers: "+engineSettings_->describe().dump());
    }
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession && !runOptions_.editorSession->playing() && editorCameraFar_>0){effectiveRenderSettings_.cameraNear=editorCameraNear_;effectiveRenderSettings_.cameraFar=editorCameraFar_;}
#endif
    const unsigned samplingScale=effectiveRenderSettings_.antiAliasing==2?2:1;
    if(sceneRenderExtent_.width!=renderExtent_.width*samplingScale || sceneRenderExtent_.height!=renderExtent_.height*samplingScale)
        recreateSwapchain();
    collectGpuTiming(currentFrame_);
    workerCommandPools_->resetFrame(currentFrame_, inFlightFences_[currentFrame_]);

#if AZURE_WITH_EDITOR
    if (runOptions_.editorSession != nullptr) {
        if (runOptions_.editorSession->consumeAssetReloadRequest()) {
            vkCheck(vkDeviceWaitIdle(device_), "vkDeviceWaitIdle(asset reload)");
#if AZURE_WITH_EDITOR
            invalidatePreviews();graphicsCompleted_=graphicsSubmission_;renderViews_->complete(graphicsCompleted_);
#endif
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
#endif

    if (levelSession_ && levelSession_->poll()) {
        runOptions_.sceneDocument = runtime_.snapshotScene();
        azurerender::RuntimeDiagnostics::instance().info("runtime", "Level committed: " + runOptions_.sceneDocument->sceneId);
    }

    std::uint32_t imageIndex = 0;
    const auto acquireStart = std::chrono::steady_clock::now();
    const VkResult acquireResult = vkAcquireNextImageKHR(
        device_,
        swapchain_,
        UINT64_MAX,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex);

    if (runOptions_.gpuTimingEnabled)
        submissionCounters_.acquireMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - acquireStart).count();
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
    azurerender::TransientResourceLease readback{};
    if (captureThisFrame) {
        if (!capturePool_) capturePool_ = std::make_unique<azurerender::TransientResourcePool>(gpuAllocator_);
        const VkDeviceSize screenshotSize =
            static_cast<VkDeviceSize>(swapchainExtent_.width)
            * swapchainExtent_.height
            * 4;
        if (readbackBufferSize_ != screenshotSize) {
            capturePool_->trim();
            readbackBufferSize_ = screenshotSize;
        }
        readback = capturePool_->acquireBuffer(
            screenshotSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, ++captureSerial_);
    }

    if (runOptions_.technicalSequence) {
        updateTechnicalSequenceState(capturedFrames_);
    }
#if AZURE_WITH_EDITOR
    if (editorLayer_ != nullptr) {
        editorLayer_->setViewportImageIndex(imageIndex);
        editorLayer_->setPreviewSubmission(graphicsSubmission_+1);
        editorLayer_->setCameraState(cameraPosition_,cameraTarget_);
        const auto view=lookAt(cameraPosition_,cameraTarget_,{0,1,0});
        const auto projection=azurerender::characterProjection(effectiveRenderSettings_,static_cast<float>(renderExtent_.width)/renderExtent_.height);
        runOptions_.editorSession->context().setViewportCameraMatrices(view,projection);
        runOptions_.editorSession->context().setDebugProjection(multiply(projection,view));
        editorLayer_->newFrame();
        editorLayer_->drawPanels();
        azurerender::EditorViewportInput viewportInput =
            editorLayer_->consumeViewportInput();
        auto& session=*runOptions_.editorSession;
        if(session.consumeFrameSelection() && !session.playing()) {
            const auto description=session.context().scene().renderDescription();
            const auto* rendererState=sceneRenderer_?sceneRenderer_->sceneState():nullptr;
            const auto bounds=azurerender::SelectionBounds::resolve(description,session.selection().selected(),[&](const std::string& resource)->std::optional<azurerender::scene::AxisAlignedBounds>{
                if(rendererState)for(const auto& entry:rendererState->pickables) {
                    const auto node=std::find_if(description.nodes.begin(),description.nodes.end(),[&](const auto& n){return n.id==entry.node;});
                    if(node!=description.nodes.end() && node->resourceId==resource && entry.asset)return azurerender::scene::AxisAlignedBounds{entry.asset->boundsMin,entry.asset->boundsMax};
                }
                return {};
            });
            const float fov=2*std::atan(std::tan(3.14159265F/6)/azurerender::internal::kCharacterLensScale);
            if(bounds.valid){
                const float radius=.5F*vectorLength(subtract(bounds.bounds.maximum,bounds.bounds.minimum));
                editorCameraNear_=std::max(.00001F,std::min(effectiveRenderSettings_.cameraNear,radius*.001F));
                editorCameraFar_=std::max(effectiveRenderSettings_.cameraFar,radius*100.F);
                effectiveRenderSettings_.cameraNear=editorCameraNear_;effectiveRenderSettings_.cameraFar=editorCameraFar_;
            }
            const auto framed=azurerender::EditorCameraService::frameSelection(bounds,cameraPosition_,cameraTarget_,fov,
                static_cast<float>(renderExtent_.width)/renderExtent_.height,effectiveRenderSettings_.cameraNear,effectiveRenderSettings_.cameraFar);
            if(framed.passed){cameraPosition_=framed.position;cameraTarget_=framed.target;autoRotate_=false;}
            else session.log("ERROR: "+framed.diagnostic);
        }
        if (!runOptions_.editorSession->playing() && azurerender::EditorCameraController::apply(
                viewportInput,
                cameraPosition_,
                cameraTarget_)) {
            autoRotate_ = false;
        }
        editorLayer_->setCameraState(cameraPosition_,cameraTarget_);
        if (viewportInput.pickRequested) {
            pendingPickRequested_ = true;
            pendingPickAdditive_ = viewportInput.pickAdditive;
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
#endif
    synchronizeEditorRuntime();synchronizeGameUi();
    azurerender::SceneFrameData frameData;
    buildSceneFrameData(frameData);
    const azurerender::RenderFrameSnapshot frameSnapshot(frameData);
    if (sceneRenderer_ != nullptr) {
        sceneRenderer_->updateFrame(frameSnapshot.frame());
    }
    updateGizmoScreenData();
    if (pendingPickRequested_) {
        pendingPickRequested_ = false;
        pickPrimitive(pendingPickX_, pendingPickY_);
    }
    // The frame fence retired this window's previous contents, so the ring
    // cursor restarts at the window base before any upload allocates.
    uploadRing_.beginFrame(static_cast<std::uint32_t>(currentFrame_));
    updateHudBuffer(currentFrame_);
    updateGameUi(frameData.deltaSeconds);
    vkCheck(vkResetFences(device_, 1, &inFlightFences_[currentFrame_]), "vkResetFences");
    vkCheck(vkResetCommandBuffer(commandBuffers_[currentFrame_], 0), "vkResetCommandBuffer");
    recordCommandBuffer(
        commandBuffers_[currentFrame_],
        imageIndex,
        captureThisFrame
            ? readback.buffer.buffer
            : VK_NULL_HANDLE,
        frameSnapshot.frame());

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
    const auto submitStart = std::chrono::steady_clock::now();
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame_]),
        "vkQueueSubmit");
    graphicsFrameSubmissions_[currentFrame_]=++graphicsSubmission_;
    if (runOptions_.gpuTimingEnabled)
        submissionCounters_.submitMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - submitStart).count();
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
                readback.buffer.mapped,
                swapchainExtent_.width,
                swapchainExtent_.height,
                outputPath);
            ++validationScreenshots_;
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
            capturePool_->retireFrame(readback.frame);
            pendingScreenshotLabel_.clear();
        } catch (...) {
            throw;
        }
    }

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &imageIndex;

    const auto presentStart = std::chrono::steady_clock::now();
    const VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    if (runOptions_.gpuTimingEnabled)
        submissionCounters_.presentMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - presentStart).count();
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

    if (runOptions_.gpuTimingEnabled) {
        ++submissionCounters_.completedCpuFrames;
        submissionCounters_.cpuFrameMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart).count();
        const double total=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frameStart).count();
        const double wait=submissionCounters_.frameSlotWaitMilliseconds+submissionCounters_.acquireMilliseconds
            +submissionCounters_.submitMilliseconds+submissionCounters_.presentMilliseconds-previousWait;
        if(submissionCounters_.workSamplesMs.size()>=8192){
            submissionCounters_.workSamplesMs.pop_front();submissionCounters_.totalSamplesMs.pop_front();submissionCounters_.waitSamplesMs.pop_front();
            for(std::size_t count=0;count<submissionCounters_.physicsStepCounts.front();++count)submissionCounters_.physicsSamplesMs.pop_front();
            submissionCounters_.physicsStepCounts.pop_front();
        }
        submissionCounters_.totalSamplesMs.push_back(total);submissionCounters_.waitSamplesMs.push_back(wait);
        submissionCounters_.workSamplesMs.push_back(std::max(0.0,total-wait));
        auto* game=activeGame();
        submissionCounters_.physicsStepCounts.push_back(game?game->lastStepSamples().size():0);
        if(game)for(double sample:game->lastStepSamples())submissionCounters_.physicsSamplesMs.push_back(sample);
        const auto revision=activeRuntime()?activeRuntime()->sceneRevision():0;
        if(gameplayFrame_%240==0 || revision!=sampledLevelRevision_){
            const auto& allocation=gpuAllocator_.statistics();
            azurerender::ResourceFrameSample sample;
            sample.frame=gameplayFrame_;sample.revision=revision;
            sample.workMs=std::max(0.0,total-wait);sample.waitMs=wait;
            sample.physicsMaxMs=game && !game->lastStepSamples().empty()?*std::max_element(game->lastStepSamples().begin(),game->lastStepSamples().end()):0;
            sample.committed=revision!=sampledLevelRevision_;
            sample.commitMs=levelSession_?levelSession_->lastCommitMilliseconds():0;
            sample.loading=levelSession_ && levelSession_->loading();
            sample.cachedCandidates=levelSession_?levelSession_->cachedCandidates():0;
            sample.requestGeneration=levelSession_?levelSession_->requestGeneration():0;
            sample.loadError=levelSession_?levelSession_->lastError():std::string();
            sample.buffers=allocation.liveBuffers;sample.images=allocation.liveImages;
            sample.bufferBytes=allocation.liveBufferBytes;sample.imageBytes=allocation.liveImageBytes;
            resourceFrameSamples_.record(sample);
            sampledLevelRevision_=revision;
        }
    }
    currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
}


void AzureRenderApp::updateGizmoScreenData() {
#if AZURE_WITH_EDITOR

    if (runOptions_.editorSession == nullptr) {
        return;
    }
    auto& editorContext = runOptions_.editorSession->context();
    editorContext.setDebugProjection(multiply(azurerender::characterProjection(effectiveRenderSettings_,
        static_cast<float>(renderExtent_.width)/renderExtent_.height),lookAt(cameraPosition_,cameraTarget_,{0,1,0})));
    editorContext.setViewportCameraMatrices(lookAt(cameraPosition_,cameraTarget_,{0,1,0}),
        azurerender::characterProjection(effectiveRenderSettings_,static_cast<float>(renderExtent_.width)/renderExtent_.height));
    editorContext.syncComponents();
    if (!ecsRenderableLogged_) {
        azurerender::RuntimeDiagnostics::instance().print(
            "ecs",
            "ECS visible renderables: "
                + std::to_string(editorContext.visibleRenderableCount()));
        ecsRenderableLogged_ = true;
    }
    editorContext.setGizmoScreen({});
    editorContext.setPickTargets({});
    const azurerender::RendererSceneState* sceneState =
        sceneRenderer_ != nullptr ? sceneRenderer_->sceneState() : nullptr;
    if (sceneState == nullptr || sceneState->asset == nullptr
        || sceneState->modelMatrix == nullptr) {
        return;
    }
    const LoadedAsset& asset = *sceneState->asset;
    std::map<std::string,azurerender::scene::AxisAlignedBounds> assetBounds;
    std::map<std::string,std::array<float,3>> pickTargets;
    for(const auto& entry:sceneState->pickables)if(entry.asset) {
        const auto& mesh=*entry.asset;
        const auto node=std::find_if(editorContext.scene().nodes.begin(),editorContext.scene().nodes.end(),[&](const auto& value){return value.id==entry.node;});
        if(node!=editorContext.scene().nodes.end())assetBounds[node->resourceId]={mesh.boundsMin,mesh.boundsMax};
        const Vector3 center={(mesh.boundsMin[0]+mesh.boundsMax[0])*.5F,(mesh.boundsMin[1]+mesh.boundsMax[1])*.5F,(mesh.boundsMin[2]+mesh.boundsMax[2])*.5F};
        pickTargets[entry.node]=transformPosition(entry.model,center);
    }
    editorContext.setPickTargets(std::move(pickTargets));
    editorContext.setSelectionAssetBounds(std::move(assetBounds));
    if (!editorContext.isProject() && (selectedPrimitiveIndex_ < 0
        || static_cast<std::size_t>(selectedPrimitiveIndex_)
            >= asset.primitives.size())) {
        return;
    }
    Matrix4 currentModel{};
    std::memcpy(currentModel.data(), sceneState->modelMatrix, sizeof(Matrix4));
    const float aspect =
        static_cast<float>(renderExtent_.width)
        / static_cast<float>(renderExtent_.height);
    const Matrix4 view = lookAt(
        cameraPosition_, cameraTarget_, {0.0F, 1.0F, 0.0F});
    const Matrix4 projection =
        azurerender::characterProjection(effectiveRenderSettings_,aspect);
    const Matrix4 viewProj = multiply(projection, view);
    const std::array<float, 3>& translation =
        editorContext.gizmoTranslation();
    Vector3 gizmoCenter=translation;
    if(editorContext.isProject()) {
        if(!editorContext.selectedNode())return;
        const auto description=editorContext.scene().renderDescription();
        const auto transforms=azurerender::scene::resolveNodeWorldTransforms(description);
        gizmoCenter=transformPosition(transforms.at(editorContext.selectedNodeIndex()),{0,0,0});
    } else {
        const auto center=transformPosition(currentModel,asset.primitives[selectedPrimitiveIndex_].center);
        for(unsigned axis=0;axis<3;++axis)gizmoCenter[axis]+=center[axis];
    }
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
    const auto screen=azurerender::EditorWorkspace::ndcToScreen(centerNdc[0],centerNdc[1]);
    data.centerX=screen[0];data.centerY=screen[1];
    const auto projectedAxis=[](float x,float y){return std::array<float,2>{x,y};};
    const std::array<float, 2> axisXScreen = projectedAxis(
        (endX[0] - centerNdc[0]) * 0.5F,
        (endX[1] - centerNdc[1]) * 0.5F);
    const std::array<float, 2> axisYScreen = projectedAxis(
        (endY[0] - centerNdc[0]) * 0.5F,
        (endY[1] - centerNdc[1]) * 0.5F);
    const std::array<float, 2> axisZScreen = projectedAxis(
        (endZ[0] - centerNdc[0]) * 0.5F,
        (endZ[1] - centerNdc[1]) * 0.5F);
    data.axisXScreenX = axisXScreen[0];
    data.axisXScreenY = axisXScreen[1];
    data.axisYScreenX = axisYScreen[0];
    data.axisYScreenY = axisYScreen[1];
    data.axisZScreenX = axisZScreen[0];
    data.axisZScreenY = axisZScreen[1];
    data.pixelToWorld = 0.005F;
    editorContext.setGizmoScreen(data);

#endif
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
#if AZURE_WITH_EDITOR
    if(runOptions_.editorSession && runOptions_.editorSession->context().isProject()) {
        std::string selected;
        for(const auto& entry:sceneState->pickables) {
            const auto& mesh=*entry.asset;
            for(std::size_t index=0;index+2<mesh.indices.size();index+=3) {
                const auto a=transformPosition(entry.model,azurerender::deformedVertexPosition(mesh,mesh.indices[index],entry.pose,entry.morph));
                const auto b=transformPosition(entry.model,azurerender::deformedVertexPosition(mesh,mesh.indices[index+1],entry.pose,entry.morph));
                const auto c=transformPosition(entry.model,azurerender::deformedVertexPosition(mesh,mesh.indices[index+2],entry.pose,entry.morph));
                const float distance=rayTriangleDistance(cameraPosition_,direction,a,b,c);
                if(distance>0 && distance<bestDistance){bestDistance=distance;selected=entry.node;}
            }
        }
        if(runOptions_.editorSession->references().active()) {
            if(!selected.empty())runOptions_.editorSession->edit("reference.deliver",{{"kind","node"},{"id",selected}});
        }else runOptions_.editorSession->edit("selection.click",{{"id",selected},{"ctrl",pendingPickAdditive_}});
        return;
    }
#endif
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
    // Vertices are composed in scratch and uploaded as one ring slice so the
    // allocation carries the exact byte count of this frame's HUD.
    hudScratch_.resize(kMaxHudVertices);
    auto* destination = hudScratch_.data();
    std::uint32_t vertexCount = 0;
    const auto uploadHud = [&]() {
        hudVertexCounts_[frameIndex] = vertexCount;
        if (vertexCount == 0) {
            return;
        }
        const VkDeviceSize bytes = sizeof(HudVertex) * vertexCount;
        const auto slice = uploadRing_.allocate(bytes);
        std::memcpy(slice.mapped, hudScratch_.data(), bytes);
        hudVertexOffsets_[frameIndex] = slice.offset;
    };
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
        uploadHud();
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
         << "  VIEW: " << kDiagnosticNames[effectiveRenderSettings_.diagnosticView]
         << "  STYLE: " << (effectiveRenderSettings_.stylizedLightingEnabled ? "ON" : "OFF")
         << "  OUTLINE: " << (effectiveRenderSettings_.innerOutlineEnabled ? "ON" : "OFF")
         << '\n';
    if (sceneRenderer_ != nullptr) {
        sceneRenderer_->appendHudText(text);
    }
    text << "TOON : RAMP V1 / 10 CLASSES  MASK "
         << std::fixed << std::setprecision(2) << effectiveRenderSettings_.styleMaskStrength
         << "  LEGACY THRESHOLD " << effectiveRenderSettings_.diffuseBandThreshold << '\n';
#if AZURE_WITH_EDITOR
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
        text << "\nINSPECTOR: OUTLINE " << effectiveRenderSettings_.outline.strength
             << "  EXPOSURE " << effectiveRenderSettings_.grade.exposureEv
             << "  PRESET " << effectiveRenderSettings_.showcasePreset << '\n'
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
#endif
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
    uploadHud();
}



void AzureRenderApp::recordCommandBuffer(
    const VkCommandBuffer commandBuffer,
    const std::uint32_t imageIndex,
    const VkBuffer screenshotBuffer,
    [[maybe_unused]] const azurerender::SceneFrameData& frame) {
    const auto recordingStart = std::chrono::steady_clock::now();
    azurerender::RenderContext sceneContext;
    buildRenderContext(sceneContext);
    sceneContext.currentFrame = static_cast<std::uint32_t>(currentFrame_);
    sceneContext.imageIndex = imageIndex;
    sceneContext.commandBuffer = commandBuffer;
    azurerender::rhi::VulkanCommandRecorder commandRecorder(commandBuffer);
    sceneContext.commands = &commandRecorder;
    sceneContext.sceneFramebuffer = swapchainFramebuffers_[imageIndex];
    if (runOptions_.gpuTimingEnabled && !timestampQueryPools_.empty()) {
        sceneContext.timestampQueryPool =
            timestampQueryPools_[currentFrame_];
    }
    sceneContext.timestampQueryCount = kTimestampQueryCount;
    sceneContext.gpuTimingEnabled = runOptions_.gpuTimingEnabled;
    // Submission counters ride along with GPU timing so a performance baseline
    // run also reports CPU-side submission cost without a separate flag.
    azurerender::SceneSubmissionCounters frameCounters;
    if (runOptions_.gpuTimingEnabled) {
        sceneContext.submissionCounters = &frameCounters;
    }
    azurerender::RenderGraph graph;
    const auto importAttachment = [&](const char* name, VkImage image, VkImageAspectFlags aspect) {
        azurerender::rhi::ImageBarrierDesc state{};
        state.image = image;
        state.aspectMask = aspect;
        return graph.importImage(name, state);
    };
    const azurerender::SceneGraphResources resources{
        importAttachment("scene-color", sceneColorImages_[imageIndex].image, VK_IMAGE_ASPECT_COLOR_BIT),
        importAttachment("scene-depth", depthImages_[imageIndex].image, VK_IMAGE_ASPECT_DEPTH_BIT),
        importAttachment("scene-normal", normalImages_[imageIndex].image, VK_IMAGE_ASPECT_COLOR_BIT),
        importAttachment("shadow-map", shadowImage_.image, VK_IMAGE_ASPECT_DEPTH_BIT)};
    azurerender::rhi::ImageBarrierDesc outputState{};
    outputState.image = swapchainImages_[imageIndex];
    // The attachment passes finish in PRESENT before graph capture starts.
    outputState.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    outputState.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    outputState.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    const auto output = graph.importImage("output", outputState);
    azurerender::rhi::VulkanCommandRecorder serialSceneRecorder(commandBuffer, true);
    if (runOptions_.parallelRecordingDisabled) sceneContext.commands = &serialSceneRecorder;
    if (sceneRenderer_ != nullptr) sceneRenderer_->registerPasses(graph, resources, sceneContext);
#if AZURE_WITH_EDITOR
    // All views consume the same snapshot after the host advances simulation.
    if(renderViews_)renderViews_->schedule(graph,frame,sceneContext,graphicsSubmission_+1);
#endif
    const auto scenePassCount = graph.passes().size();
    const auto composite = graph.addPass("post-process-hud", [&] {
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
        effectiveRenderSettings_.diagnosticView == 2
            ? 1.0F
            : (effectiveRenderSettings_.innerOutlineEnabled
                ? effectiveRenderSettings_.outline.strength : 0.0F),
        effectiveRenderSettings_.outline.depthThreshold,
        effectiveRenderSettings_.outline.normalThreshold,
        static_cast<float>(effectiveRenderSettings_.diagnosticView),
        effectiveRenderSettings_.grade.exposureEv,
        effectiveRenderSettings_.grade.toneMappingEnabled ? 1.0F : 0.0F,
        effectiveRenderSettings_.bloom.enabled
            && !(qaEffectName_ == "bloom" && !qaEffectEnabled_)
            ? effectiveRenderSettings_.bloom.strength : 0.0F,
        qaEffectName_ == "bloom" && qaEffectStateName_ == "isolation"
            ? 1.0F : 0.0F,
        {
            effectiveRenderSettings_.outline.color[0],
            effectiveRenderSettings_.outline.color[1],
            effectiveRenderSettings_.outline.color[2],
            1.0F,
        },
        {
            effectiveRenderSettings_.grade.saturation,
            effectiveRenderSettings_.grade.contrast,
            effectiveRenderSettings_.bloom.threshold,
            0.0F,
        },
        {
            effectiveRenderSettings_.grade.tint[0],
            effectiveRenderSettings_.grade.tint[1],
            effectiveRenderSettings_.grade.tint[2],
            0.0F,
        },
        {static_cast<float>(effectiveRenderSettings_.antiAliasing),effectiveRenderSettings_.cameraNear,
         effectiveRenderSettings_.cameraFar,effectiveRenderSettings_.antiAliasing==2?2.0F:1.0F},
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
        const VkBuffer hudBuffer = uploadRing_.buffer();
        vkCmdBindVertexBuffers(
            commandBuffer,
            0,
            1,
            &hudBuffer,
            &hudVertexOffsets_[currentFrame_]);
        vkCmdDraw(
            commandBuffer,
            hudVertexCounts_[currentFrame_],
            1,
            0,
            0);
    }
    if(gameUiRenderer_ && gameUi_){const auto before=gameUiRenderer_->drawCalls();gameUiRenderer_->record(commandRecorder);uiDrawCalls_+=gameUiRenderer_->drawCalls()-before;}
    vkCmdEndRenderPass(commandBuffer);
    });
    graph.use(composite, resources.color, azurerender::RenderGraphUsage::Sampled, false);
    graph.use(composite, resources.depth, azurerender::RenderGraphUsage::Sampled, false);
    graph.use(composite, resources.normal, azurerender::RenderGraphUsage::Sampled, false);
    graph.use(composite, resources.shadow, azurerender::RenderGraphUsage::Sampled, false);
    graph.write(composite, output);
    const auto editor = graph.addPass("editor-ui", [&] {
#if AZURE_WITH_EDITOR
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
#endif
    });
    graph.write(editor, output);
    const auto capture = graph.addPass("capture", [&] {
    if (runOptions_.gpuTimingEnabled) {
        vkCmdWriteTimestamp(
            commandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            timestampQueryPools_[currentFrame_],
            3);
    }

    if (screenshotBuffer != VK_NULL_HANDLE) {
        commandRecorder.copyImageToBuffer(swapchainImages_[imageIndex], screenshotBuffer, swapchainExtent_);
    }
    });
    if (screenshotBuffer != VK_NULL_HANDLE) {
        graph.use(capture, output, azurerender::RenderGraphUsage::TransferSrc, false);
        const auto present = graph.addPass("present");
        graph.use(present, output, azurerender::RenderGraphUsage::Present, false);
        graph.dependsOn(present, capture);
    } else {
        graph.read(capture, output);
    }
    std::string graphError;
    if (!graph.compile(graphError)) throw std::runtime_error(graphError);
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

    std::vector<std::vector<VkCommandBuffer>> recordedPasses(graph.passes().size());
    std::vector<azurerender::RecordingWorkerPool::Task> recordingTasks;
    std::size_t recordingTaskCount = 0;
    if (!runOptions_.parallelRecordingDisabled) {
        for (const auto& pass : graph.passes())
            if (pass.parallelRecording)
                recordingTaskCount += std::max<std::size_t>(1, pass.recordingChunks.size());
    }
    recordingTasks.reserve(recordingTaskCount);
    std::vector<azurerender::RenderGraph::PassId> recordingOrder;
    if (!runOptions_.parallelRecordingDisabled) {
        recordingOrder.reserve(graph.executionOrder().size());
        for (const auto pass : graph.executionOrder())
            if (graph.passes()[pass].parallelRecording)
                recordingOrder.push_back(pass);
    }
    for (const auto pass : recordingOrder) {
        if (runOptions_.parallelRecordingDisabled
            || !graph.passes()[pass].parallelRecording) continue;
        const auto chunkCount = std::max<std::size_t>(1, graph.passes()[pass].recordingChunks.size());
        recordedPasses[pass].resize(chunkCount, VK_NULL_HANDLE);
        for (std::size_t chunk = 0; chunk < chunkCount; ++chunk) {
        recordingTasks.push_back([&, pass, chunk](std::size_t worker) {
            const auto secondary = workerCommandPools_->allocate(currentFrame_, worker);
            VkCommandBufferInheritanceInfo inheritance{
                VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
            inheritance.renderPass = graph.passes()[pass].renderPass.renderPass;
            inheritance.framebuffer = graph.passes()[pass].renderPass.framebuffer;
            VkCommandBufferBeginInfo secondaryBegin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
            secondaryBegin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            if (inheritance.renderPass != VK_NULL_HANDLE)
                secondaryBegin.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            secondaryBegin.pInheritanceInfo = &inheritance;
            vkCheck(vkBeginCommandBuffer(secondary, &secondaryBegin), "vkBeginCommandBuffer(worker)");
            azurerender::rhi::VulkanCommandRecorder workerRecorder(secondary, true);
            if (graph.passes()[pass].recordingChunks.empty()) graph.passes()[pass].recordCommands(workerRecorder);
            else graph.passes()[pass].recordingChunks[chunk](workerRecorder);
            vkCheck(vkEndCommandBuffer(secondary), "vkEndCommandBuffer(worker)");
            recordedPasses[pass][chunk] = secondary;
        });
        }
    }
    const auto workerStart = std::chrono::steady_clock::now();
    recordingWorkers_.runAdaptive(std::move(recordingTasks));
    const auto executionStart = std::chrono::steady_clock::now();
    graph.execute(&commandRecorder, [&](azurerender::RenderGraph::PassId pass) {
        if (recordedPasses[pass].empty()) {
            if (runOptions_.parallelRecordingDisabled && pass < scenePassCount
                && graph.passes()[pass].recordCommands) {
                const auto& scenePass = graph.passes()[pass];
                if (scenePass.graphicsPass) serialSceneRecorder.beginRenderPass(scenePass.renderPass);
                scenePass.recordCommands(serialSceneRecorder);
                if (scenePass.graphicsPass) serialSceneRecorder.endRenderPass();
                return true;
            }
            return false;
        }
        const auto& graphics = graph.passes()[pass].renderPass;
        if (graphics.renderPass != VK_NULL_HANDLE) {
            VkRenderPassBeginInfo renderBegin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            renderBegin.renderPass = graphics.renderPass;
            renderBegin.framebuffer = graphics.framebuffer;
            renderBegin.renderArea.extent = graphics.extent;
            renderBegin.clearValueCount = static_cast<std::uint32_t>(graphics.clearValues.size());
            renderBegin.pClearValues = graphics.clearValues.data();
            vkCmdBeginRenderPass(commandBuffer, &renderBegin, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        }
        vkCmdExecuteCommands(commandBuffer, static_cast<std::uint32_t>(recordedPasses[pass].size()), recordedPasses[pass].data());
        serialSceneRecorder.invalidatePipelineBindings();
        if (graphics.renderPass != VK_NULL_HANDLE) vkCmdEndRenderPass(commandBuffer);
        return true;
    }, runOptions_.parallelRecordingDisabled);
    if (runOptions_.gpuTimingEnabled) {
        const auto executionEnd = std::chrono::steady_clock::now();
        submissionCounters_.graphPreparationMilliseconds +=
            std::chrono::duration<double, std::milli>(workerStart - recordingStart).count();
        submissionCounters_.workerWaitMilliseconds +=
            std::chrono::duration<double, std::milli>(executionStart - workerStart).count();
        submissionCounters_.graphExecutionMilliseconds +=
            std::chrono::duration<double, std::milli>(executionEnd - executionStart).count();
        ++submissionCounters_.frames;
        submissionCounters_.instances += frameCounters.instances;
        submissionCounters_.indirectDrawCalls += frameCounters.indirectDrawCalls;
        submissionCounters_.visibleInstances += frameCounters.visibleInstances;
        submissionCounters_.recordingMilliseconds +=
            std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - recordingStart).count();
        submissionCounters_.workerRecordedPasses += static_cast<std::uint64_t>(
            std::count_if(recordedPasses.begin(), recordedPasses.end(),
                [](const auto& buffers) { return !buffers.empty(); }));
        for (const auto& buffers : recordedPasses)
            submissionCounters_.workerRecordedChunks += buffers.size();
        submissionCounters_.drawCalls += frameCounters.drawCalls;
        submissionCounters_.descriptorSetBinds +=
            frameCounters.descriptorSetBinds;
        submissionCounters_.pipelineBinds += frameCounters.pipelineBinds;
        submissionCounters_.pushConstantUpdates +=
            frameCounters.pushConstantUpdates;
    }

    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}
