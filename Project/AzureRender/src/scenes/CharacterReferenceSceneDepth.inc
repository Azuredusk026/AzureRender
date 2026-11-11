namespace {
bool referenceDepthRequested(const RenderContext& context){
    if(context.referenceSceneDepthPrograms)return true;
    if(!context.renderSettings||!context.renderSettings->cameraView)return false;
    const auto reference=resolveReferenceCamera(*context.renderSettings->cameraView);
    return reference&&reference->descriptor().produceSceneDepth;
}

std::vector<std::array<std::uint32_t,3>> referenceDepthSpans(const std::vector<scene::SceneInstance>& instances){
    std::vector<std::array<std::uint32_t,3>> spans;
    bool previousMirrored=false;
    for(const auto& instance:instances){
        const bool mirrored=scene::mirroredTransform(instance.model);
        if(!spans.empty()&&spans.back()[0]==instance.meshKey&&previousMirrored==mirrored
            &&std::uint64_t(spans.back()[1])+spans.back()[2]==instance.sourceIndex){
            ++spans.back()[2];
        }else{
            spans.push_back({instance.meshKey,instance.sourceIndex,1});
            previousMirrored=mirrored;
        }
    }
    return spans;
}
}

bool CharacterSceneRenderer::referenceDepthActive() const noexcept {
    return referenceCamera_&&referenceCamera_->descriptor().produceSceneDepth;
}

std::optional<SceneReferenceDepthCapture> CharacterSceneRenderer::readReferenceSceneDepth() const {
    if(!referenceSceneDepth_||!submittedReferenceDepth_)return std::nullopt;
    const auto [slot,submission]=*submittedReferenceDepth_;
    return referenceSceneDepth_->read(slot,submission);
}

void CharacterSceneRenderer::createReferenceDepthPipelines(rhi::GraphicsPipelineDesc desc,const RenderContext& context){
    validateMaterialUvGradientPolicy(MaterialUvGradientPolicy::MirroredCoarseY,queryMaterialUvGradientSupport(context.physicalDevice));
    const auto fragment=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+(bindlessTextures_
        ?"/reference_scene_depth_bindless.frag.spv":"/reference_scene_depth.frag.spv")));
    try{
        desc.vertexSpecialization.push_back({7,VK_TRUE});
        desc.fragmentShader=fragment;
        desc.renderPass=referenceSceneDepth_->renderPass();
        desc.colorAttachmentCount=1;
        desc.colorWriteMasks={VK_COLOR_COMPONENT_R_BIT};
        desc.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        desc.minSampleShading=0;
        desc.depthTest=true;
        desc.depthWrite=true;
        desc.depthCompare=VK_COMPARE_OP_GREATER;
        for(unsigned facing=0;facing<referenceSceneDepthPipelines_.size();++facing){
            // The reference attachment stores rows in the opposite order to the scene view.
            desc.frontFace=facing==1?VK_FRONT_FACE_COUNTER_CLOCKWISE:VK_FRONT_FACE_CLOCKWISE;
            desc.cullMode=context.faceCullingEnabled&&facing!=2?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
            referenceSceneDepthPipelines_[facing]=rhi_->createGraphicsPipeline(desc);
        }
    }catch(...){
        rhi_->destroyShaderModule(fragment);
        throw;
    }
    rhi_->destroyShaderModule(fragment);
}
