void CharacterSceneRenderer::createScreenReflectionResources(const RenderContext& context){
    destroyScreenReflectionResources();
    rhi::GpuBuffer staging{};
    try{
        staging=allocator_->createBuffer(16,VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);std::memset(staging.mapped,0,16);allocator_->flush(staging,0,16);
        reflectionFallback_=allocator_->createImage2D(1,1,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        rhi_->transitionImageLayout(reflectionFallback_,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
        rhi_->copyBufferToImage(staging,reflectionFallback_,1,1,1,16);
        rhi_->executeOneShot([&](rhi::ICommandRecorder& commands){commands.imageBarrier({reflectionFallback_.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,1,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});});
        allocator_->destroyBuffer(staging);reflectionFallbackView_=rhi_->createImageView(reflectionFallback_.image,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
        reflectionPreparation_="initial-setting-disabled";
        reflectionAlgorithmRequest_=context.renderSettings?context.renderSettings->screenSpaceReflection.algorithm:ScreenSpaceReflectionAlgorithm::Linear;
        if(!context.renderSettings||!context.renderSettings->screenSpaceReflection.enabled)return;
        reflectionPreparation_="queue-without-compute";if(!context.computeShaders)return;
        reflectionPreparation_="depth-only-snapshot";if(reflectionSurfaceFormat_!=VK_FORMAT_R32G32B32A32_SFLOAT)return;
        constexpr unsigned resolutionDivisor=2;
        if(reflectionAlgorithmRequest_==ScreenSpaceReflectionAlgorithm::Projected){
            if(ProjectedScreenReflectionGpu::supported(*rhi_,context.renderExtent,kMaxFramesInFlight,7,resolutionDivisor)){
                try{
                    projectedReflections_=std::make_unique<ProjectedScreenReflectionGpu>(*rhi_,context.renderExtent,static_cast<unsigned>(kMaxFramesInFlight),
                        ProjectedScreenReflectionShaders::load(shaderDirectory_),7,resolutionDivisor);
                    reflectionPreparation_="projected-compute";
                    reflectionHistoryPreparation_="multisample-motion-unavailable";
                    const auto pixels=std::uint64_t(context.renderExtent.width)*context.renderExtent.height;
                    if(context.rasterizationSamples==VK_SAMPLE_COUNT_1_BIT){
                        if(context.descriptorLimits.maxPerStageDescriptorStorageBuffers&&context.descriptorLimits.maxPerStageDescriptorStorageBuffers<6)
                            reflectionHistoryPreparation_="geometry-storage-limits";
                        else if(!context.motionOutput&&pixels>MotionVectorTarget::kMaximumBytes/MotionVectorTarget::kBytesPerPixel)
                            reflectionHistoryPreparation_="motion-budget";
                        else try{
                            createReflectionMotionResources(context);projectedReflectionHistoryPrepared_=true;
                            reflectionHistoryPreparation_="projected-temporal";
                        }catch(const std::exception& error){destroyReflectionHistoryResources();reflectionHistoryPreparation_=error.what();}
                    }
                    return;
                }catch(const std::exception& error){projectedReflections_.reset();reflectionFallbackReason_=error.what();}
            }else reflectionFallbackReason_="projected-formats-or-budget";
        }
        reflectionPreparation_="resource-limits";if(!ScreenReflectionGpu::supported(*rhi_,context.renderExtent,kMaxFramesInFlight,7,ReflectionPyramidPrecision::PreferPacked,resolutionDivisor))return;
        bool temporal=context.rasterizationSamples==VK_SAMPLE_COUNT_1_BIT;
        reflectionHistoryPreparation_=temporal?"resource-limits":"multisample-motion-unavailable";
        if(temporal&&context.descriptorLimits.maxPerStageDescriptorStorageBuffers&&context.descriptorLimits.maxPerStageDescriptorStorageBuffers<6){temporal=false;reflectionHistoryPreparation_="geometry-storage-limits";}
        const auto pixels=std::uint64_t(context.renderExtent.width)*context.renderExtent.height;
        if(temporal&&!context.motionOutput&&pixels>MotionVectorTarget::kMaximumBytes/MotionVectorTarget::kBytesPerPixel){temporal=false;reflectionHistoryPreparation_="motion-budget";}
        const auto halfPixels=std::uint64_t(std::max(1U,context.renderExtent.width/2))*std::max(1U,context.renderExtent.height/2);
        if(temporal&&halfPixels*kMaxFramesInFlight>ReflectionReprojectionGpu::kMaximumBytes/ReflectionReprojectionGpu::kBytesPerPixel){temporal=false;reflectionHistoryPreparation_="history-budget";}
        if(temporal&&!ScreenReflectionGpu::supported(*rhi_,context.renderExtent,kMaxFramesInFlight,7,ReflectionPyramidPrecision::PreferPacked,resolutionDivisor,true)){temporal=false;reflectionHistoryPreparation_="combined-budget";}
        const auto shaders=ScreenReflectionShaders::load(shaderDirectory_);
        if(temporal){
            try{
                createReflectionMotionResources(context);
                reflectionReprojection_=std::make_unique<ReflectionReprojectionGpu>(*rhi_,context.renderExtent,static_cast<unsigned>(kMaxFramesInFlight),readBinaryFile(shaderDirectory_+"/reflection_reprojection.comp.spv"),resolutionDivisor);
                screenReflections_=std::make_unique<ScreenReflectionGpu>(*rhi_,context.renderExtent,static_cast<unsigned>(kMaxFramesInFlight),shaders,7,ReflectionPyramidPrecision::PreferPacked,resolutionDivisor,true);
                reflectionHistoryPreparation_="temporal";
            }catch(const std::exception& error){destroyReflectionHistoryResources();screenReflections_.reset();temporal=false;reflectionHistoryPreparation_=error.what();}
        }
        if(!temporal)screenReflections_=std::make_unique<ScreenReflectionGpu>(*rhi_,context.renderExtent,static_cast<unsigned>(kMaxFramesInFlight),shaders,7,ReflectionPyramidPrecision::PreferPacked,resolutionDivisor);
        reflectionPreparation_="compute";
    }catch(...){allocator_->destroyBuffer(staging);destroyScreenReflectionResources();throw;}
}
void CharacterSceneRenderer::destroyScreenReflectionResources(){
    projectedReflections_.reset();screenReflections_.reset();reflectionDescriptorPublished_.fill(false);reflectionActive_.fill(false);reflectionShadingActive_.fill(false);reflectionFallbackReason_.clear();
    destroyReflectionHistoryResources();
    if(reflectionFallbackView_)rhi_->destroyImageView(reflectionFallbackView_);reflectionFallbackView_=VK_NULL_HANDLE;
    allocator_->destroyImage(reflectionFallback_);
}
void CharacterSceneRenderer::destroyReflectionHistoryResources(){
    for(auto& framebuffer:reflectionMotionFramebuffers_){if(framebuffer)rhi_->destroyFramebuffer(framebuffer);framebuffer=VK_NULL_HANDLE;}
    reflectionReprojection_.reset();reflectionOwnedMotion_.reset();reflectionPendingSlot_.reset();reflectionPendingView_.reset();projectedReflectionHistoryPrepared_=false;
    reflectionHistoryView_={};reflectionPendingJitterUv_={};reflectionHistoryJitterUv_={};reflectionMotionUvOffsets_={};reflectionInputSubmissions_={};
    reflectionHistorySubmission_=0;reflectionHistorySlot_=0;reflectionHistoryValid_.fill(false);reflectionHistoryPreparation_="not-requested";
}
void CharacterSceneRenderer::createReflectionMotionResources(const RenderContext& context){
    if(!context.motionOutput)reflectionOwnedMotion_=std::make_unique<MotionVectorTarget>(*rhi_,*allocator_,context.renderExtent,context.depthFormat,
        std::vector<VkImageView>(overlayDepthViews_.begin(),overlayDepthViews_.end()));
    auto* motion=context.motionOutput?context.motionOutput:reflectionOwnedMotion_.get();
    if(context.motionOutput)for(unsigned frame=0;frame<kMaxFramesInFlight;++frame)
        reflectionMotionFramebuffers_[frame]=rhi_->createFramebuffer({motion->renderPass(),{motion->view(),motion->surfaceDepthView(),overlayDepthViews_[frame]},context.renderExtent.width,context.renderExtent.height});
}
bool CharacterSceneRenderer::reflectionHistoryAvailable(unsigned slot) const noexcept{
    const auto* temporal=state_.temporalFrame;
    return temporal&&temporal->previous&&reflectionHistorySubmission_&&temporal->previousSubmission==reflectionHistorySubmission_&&reflectionHistorySlot_!=slot;
}
VkFramebuffer CharacterSceneRenderer::reflectionMotionFramebuffer(unsigned slot) const{
    return reflectionOwnedMotion_?reflectionOwnedMotion_->framebuffer(slot):reflectionMotionFramebuffers_.at(slot);
}
void CharacterSceneRenderer::recordScreenReflection(rhi::ICommandRecorder& commands,unsigned slot,const ReflectionTraceInputs& inputs,const ReflectionTraceView& view,const ScreenSpaceReflectionSettings& settings){
    publishScreenReflectionOutput(slot);
    if(projectedReflections_){
        ProjectedScreenReflectionParameters parameters;parameters.maxSteps=settings.maxSteps;parameters.edgeFade=settings.edgeFade;parameters.maxRoughness=settings.maxRoughness;parameters.strength=settings.strength;
        const std::array<float,2> jitterUv{samplingJitter_[0]/inputs.width,samplingJitter_[1]/inputs.height};
        const bool valid=projectedReflectionHistoryPrepared_&&reflectionHistoryAvailable(slot);
        ProjectedScreenReflectionHistory history;
        if(valid){
            history={reflectionHistorySlot_,opaqueSceneColor_[reflectionHistorySlot_].view,reflectionHistoryView_};
            for(unsigned axis=0;axis<2;++axis)parameters.motionUvOffset[axis]=jitterUv[axis]-reflectionHistoryJitterUv_[axis];
            parameters.requireTrackedMotion=true;
        }
        projectedReflections_->record(commands,slot,{inputs.surface,inputs.color,projectedReflectionHistoryPrepared_?motionTarget_->view():VK_NULL_HANDLE,{inputs.width,inputs.height}},view,parameters,valid?&history:nullptr);
        reflectionHistoryValid_[slot]=valid;reflectionMotionUvOffsets_[slot]=parameters.motionUvOffset;reflectionInputSubmissions_[slot]=valid?reflectionHistorySubmission_:0;
        if(projectedReflectionHistoryPrepared_){reflectionPendingSlot_=slot;reflectionPendingView_=view;reflectionPendingJitterUv_=jitterUv;}
        return;
    }
    if(!reflectionReprojection_){screenReflections_->record(commands,slot,inputs,view,settings);return;}
    const auto* temporal=state_.temporalFrame;
    const bool valid=reflectionHistoryAvailable(slot);
    ReflectionReprojectionParameters parameters;parameters.historyValid=valid;
    if(temporal){const auto& projection=temporal->previous?temporal->previous->projection:temporal->current.projection;parameters.previousDepthProjection={projection[10],projection[14],projection[11],projection[15]};}
    reflectionReprojection_->record(commands,slot,{inputs.surface,motionTarget_->view(),valid?reflectionReprojection_->output(reflectionHistorySlot_).motion:VK_NULL_HANDLE,inputs.width,inputs.height},parameters);
    const auto reprojection=reflectionReprojection_->output(slot);ScreenReflectionHistoryInputs history;
    history.motion=reprojection.motion;history.acceptance=reprojection.acceptance;history.parameters.historyValid=valid;
    if(valid){const auto previous=screenReflections_->history(reflectionHistorySlot_);history.previousColor=previous.color;history.previousConfidence=previous.confidence;}
    screenReflections_->record(commands,slot,inputs,view,settings,&history);
    reflectionHistoryValid_[slot]=valid;reflectionInputSubmissions_[slot]=valid?reflectionHistorySubmission_:0;reflectionPendingSlot_=slot;
}
void CharacterSceneRenderer::publishScreenReflectionOutput(unsigned frame){
    if(reflectionDescriptorPublished_.at(frame))return;
    const auto output=screenReflectionOutput(frame);const auto count=bindlessTextures_?1U:static_cast<unsigned>(totalMaterialCount());
    for(unsigned index=0;index<count;++index)rhi_->writeDescriptorImage({descriptorSets_.at(frame*count+index),37,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        output.view,VK_NULL_HANDLE,output.layout});
    reflectionDescriptorPublished_[frame]=true;
}
ReflectionFilterOutput CharacterSceneRenderer::screenReflectionOutput(unsigned slot) const{
    if(projectedReflections_)return projectedReflections_->output(slot);
    if(screenReflections_)return screenReflections_->output(slot);
    throw std::logic_error("Screen reflection output requires a prepared producer");
}
const char* CharacterSceneRenderer::screenReflectionProducer(unsigned slot) const{
    if(!reflectionShadingActive_.at(slot))return "disabled";
    if(!reflectionActive_.at(slot))return "fragment";
    if(projectedReflections_)return reflectionHistoryValid_.at(slot)?"projected-history":"projected-current-frame";
    return reflectionReprojection_?"linear-temporal":"linear-current-frame";
}
std::string CharacterSceneRenderer::screenReflectionFallback(unsigned slot) const{
    if(!reflectionShadingActive_.at(slot))return {};
    if(!screenReflectionPrepared())return reflectionPreparation_;
    if(reflectionSettings_.at(slot).algorithm!=reflectionAlgorithmRequest_)return "resource-rebuild-required";
    const auto& projection=reflectionViews_.at(slot).projection;
    if(projectedReflections_&&(projection[11]!=-1||projection[15]!=0||projection[10]>=-1||projection[14]>=0))return "perspective-finite-depth-required";
    return reflectionFallbackReason_;
}
