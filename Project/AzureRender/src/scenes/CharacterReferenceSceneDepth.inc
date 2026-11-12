namespace {
using ReferenceDepthStencil = AssetMaterial::ReferenceDepthStencil;

VkStencilOpState referenceDepthStencilState(ReferenceDepthStencil role) {
    constexpr std::uint32_t protection = 1;
    const bool respect = role == ReferenceDepthStencil::Respect;
    VkStencilOpState state{};
    state.failOp = VK_STENCIL_OP_KEEP;
    state.depthFailOp = VK_STENCIL_OP_KEEP;
    state.passOp = respect ? VK_STENCIL_OP_KEEP : VK_STENCIL_OP_REPLACE;
    state.compareOp = respect ? VK_COMPARE_OP_EQUAL : VK_COMPARE_OP_ALWAYS;
    state.compareMask = protection;
    state.writeMask = respect ? 0 : protection;
    state.reference = role == ReferenceDepthStencil::Protect ? protection : 0;
    return state;
}

bool referenceDepthRequested(const RenderContext& context){
    if(context.referenceSceneDepthPrograms||context.referenceSceneVisibilityPrograms||context.referenceSceneObjectVisibilityPrograms)return true;
    if(!context.renderSettings||!context.renderSettings->cameraView)return false;
    const auto reference=resolveReferenceCamera(*context.renderSettings->cameraView);
    return reference&&reference->descriptor().produceSceneDepth;
}

bool referenceVisibilityRequested(const RenderContext& context){
    if(context.referenceSceneVisibilityPrograms||context.referenceSceneObjectVisibilityPrograms)return true;
    if(!context.renderSettings||!context.renderSettings->cameraView)return false;
    const auto& calibration=context.renderSettings->cameraView->referenceCalibration;
    return calibration&&calibration->view.produceSceneVisibilityInputs;
}

bool referenceObjectVisibilityRequested(const RenderContext& context){
    if(context.referenceSceneObjectVisibilityPrograms)return true;
    if(!context.renderSettings||!context.renderSettings->cameraView)return false;
    const auto& calibration=context.renderSettings->cameraView->referenceCalibration;
    return calibration&&calibration->view.objectVisibility.has_value();
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

bool CharacterSceneRenderer::referenceVisibilityActive() const noexcept {
    return referenceCamera_&&referenceCamera_->descriptor().produceSceneVisibilityInputs;
}

bool CharacterSceneRenderer::referenceObjectVisibilityActive() const noexcept {
    return referenceCamera_&&referenceCamera_->descriptor().objectVisibility.has_value();
}

void CharacterSceneRenderer::writeReferenceMaterialVisibilityDescriptors(VkDescriptorSet set,unsigned slot,bool active){
    const auto visibility=active?referenceObjectVisibility_->output(slot).view:gpuMaterials_[0].baseColor.view;
    const auto identity=active?referenceSceneDepth_->objectIdView(slot):referenceObjectIdFallbackView_;
    rhi_->writeDescriptorImage({set,50,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,visibility,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    rhi_->writeDescriptorImage({set,51,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,identity,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
}

std::optional<SceneReferenceDepthCapture> CharacterSceneRenderer::readReferenceSceneDepth() const {
    if(!referenceSceneDepth_||!submittedReferenceDepth_)return std::nullopt;
    const auto& frame=*submittedReferenceDepth_;
    const VkImage visibility=frame.objectVisibility?referenceObjectVisibility_->output(frame.slot).image:VK_NULL_HANDLE;
    return referenceSceneDepth_->read(frame.slot,frame.submission,frame.visibilityInputs,visibility);
}

void CharacterSceneRenderer::recordReferenceObjectVisibility(rhi::ICommandRecorder& commands,unsigned slot,
    const SceneInstanceSnapshot& snapshot){
    if(snapshot.objectShadowPages.empty()){
        referenceObjectVisibility_->recordNeutral(commands,slot);
        return;
    }
    ScreenObjectVisibilityInputs inputs;
    inputs.depth=referenceSceneDepth_->rasterView(slot);
    inputs.normals=referenceSceneDepth_->packedNormalView(slot);
    inputs.objectIds=referenceSceneDepth_->objectIdView(slot);
    inputs.atlas=objectShadowAtlas_->view();
    inputs.extent=referenceObjectVisibility_->output(slot).extent;
    inputs.atlasFormat=VK_FORMAT_D16_UNORM;
    inputs.atlasLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    inputs.objectDepth=ScreenObjectDepth::Forward;
    const auto view=sceneReferenceObjectVisibilityView(*referenceCamera_);
    const auto& calibration=referenceCamera_->descriptor().objectVisibility->calibration;
    const auto records=sceneReferenceObjectVisibilityRecords(sceneReferenceObjectVisibilityFrame(*referenceCamera_),
        snapshot.objectShadowPages,calibration?&*calibration:nullptr);
    referenceObjectVisibility_->record(commands,slot,inputs,view,records);
}

void CharacterSceneRenderer::createReferenceDepthPipelines(rhi::GraphicsPipelineDesc desc,const RenderContext& context,bool visibilityInputs){
    validateMaterialUvGradientPolicy(MaterialUvGradientPolicy::MirroredCoarseY,queryMaterialUvGradientSupport(context.physicalDevice));
    const auto fragmentName=visibilityInputs?"/reference_scene_visibility":"/reference_scene_depth";
    const auto fragment=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+fragmentName+
        (bindlessTextures_?"_bindless.frag.spv":".frag.spv")));
    VkShaderModule visibilityVertex=VK_NULL_HANDLE;
    try{
        if(visibilityInputs){
            const auto vertexPath=shaderDirectory_+"/character_snapshot_depth_visibility"+
                (computeSkinningEnabled_?"_compute.vert.spv":".vert.spv");
            visibilityVertex=rhi_->createShaderModule(readReferenceVertexProgram(vertexPath));
            desc.vertexShader=visibilityVertex;
        }
        desc.vertexSpecialization.push_back({7,VK_TRUE});
        desc.fragmentShader=fragment;
        desc.renderPass=referenceSceneDepth_->renderPass();
        desc.colorAttachmentCount=referenceSceneDepth_->visibilityInputs()?3:1;
        desc.colorWriteMasks.assign(desc.colorAttachmentCount,0);
        desc.colorWriteMasks[0]=VK_COLOR_COMPONENT_R_BIT;
        if(visibilityInputs){
            desc.colorWriteMasks[1]=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
            desc.colorWriteMasks[2]=VK_COLOR_COMPONENT_R_BIT;
        }
        desc.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        desc.minSampleShading=0;
        desc.depthTest=true;
        desc.depthWrite=true;
        desc.depthCompare=VK_COMPARE_OP_GREATER;
        auto& pipelines=visibilityInputs?referenceSceneVisibilityPipelines_:referenceSceneDepthPipelines_;
        const bool stencilEnabled = referenceSceneDepth_->stencilEnabled();
        const auto roles = stencilEnabled ? pipelines.size() : 1;
        for (std::size_t role = 0; role < roles; ++role) {
            if (stencilEnabled)
                desc.stencil = referenceDepthStencilState(static_cast<ReferenceDepthStencil>(role));
            auto& orientations = pipelines[role];
            for (unsigned facing = 0; facing < orientations.size(); ++facing) {
                // The reference attachment stores rows in the opposite order to the scene view.
                desc.frontFace=facing==1?VK_FRONT_FACE_COUNTER_CLOCKWISE:VK_FRONT_FACE_CLOCKWISE;
                desc.cullMode=context.faceCullingEnabled&&facing!=2?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                orientations[facing]=rhi_->createGraphicsPipeline(desc);
            }
        }
    }catch(...){
        if(visibilityVertex)rhi_->destroyShaderModule(visibilityVertex);
        rhi_->destroyShaderModule(fragment);
        throw;
    }
    if(visibilityVertex)rhi_->destroyShaderModule(visibilityVertex);
    rhi_->destroyShaderModule(fragment);
}

void CharacterSceneRenderer::createReferenceOutlineDepthPipelines(rhi::GraphicsPipelineDesc desc,const RenderContext& context){
    const std::string binding=bindlessTextures_?"_bindless":"";
    const std::string skinning=computeSkinningEnabled_?"_compute":"";
    VkShaderModule vertex=VK_NULL_HANDLE,fragment=VK_NULL_HANDLE;
    try{
        vertex=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+"/reference_scene_outline_depth"+skinning+binding+".vert.spv"));
        fragment=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+"/reference_scene_depth"+binding+".frag.spv"));
        desc.vertexShader=vertex;
        desc.fragmentShader=fragment;
        desc.renderPass=referenceSceneDepth_->renderPass();
        desc.colorAttachmentCount=referenceSceneDepth_->visibilityInputs()?3:1;
        desc.colorWriteMasks.assign(desc.colorAttachmentCount,0);
        desc.colorWriteMasks[0]=VK_COLOR_COMPONENT_R_BIT;
        desc.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;
        desc.minSampleShading=0;
        desc.depthTest=true;
        desc.depthWrite=true;
        desc.depthCompare=VK_COMPARE_OP_GREATER;
        desc.cullMode=context.faceCullingEnabled?VK_CULL_MODE_FRONT_BIT:VK_CULL_MODE_NONE;
        const auto roles = referenceSceneDepth_->stencilEnabled() ? referenceSceneOutlineDepthPipelines_.size() : 1;
        for (std::size_t role = 0; role < roles; ++role) {
            if (role == 1) desc.stencil = referenceDepthStencilState(ReferenceDepthStencil::Respect);
            auto& orientations = referenceSceneOutlineDepthPipelines_[role];
            for (unsigned facing = 0; facing < orientations.size(); ++facing) {
                desc.frontFace=facing?VK_FRONT_FACE_COUNTER_CLOCKWISE:VK_FRONT_FACE_CLOCKWISE;
                orientations[facing]=rhi_->createGraphicsPipeline(desc);
            }
        }
    }catch(...){
        if(fragment)rhi_->destroyShaderModule(fragment);
        if(vertex)rhi_->destroyShaderModule(vertex);
        throw;
    }
    rhi_->destroyShaderModule(fragment);
    rhi_->destroyShaderModule(vertex);
}
