bool CharacterSceneRenderer::radianceSelectionMaterial(const AssetMaterial& material) noexcept {
    return ordinaryOpaqueMaterial(material)&&!extendedMaterialLighting(material)
        &&!material.coverageTexture.present&&!material.nprDiffuse.screenLightingNormal&&!opaqueSurfaceOverlay(material);
}
void CharacterSceneRenderer::prepareRadianceSelection(){
    auto& frame=conservativeFrames_.at(currentFrame_);frame.selectionTicket.reset();
    if(!surfaceRadiance_||!surfaceRadiance_->selectionEnabled())return;
    const bool permitted=radianceValidationPass_&&frame.ticket&&qaIsolationMode_==0&&qaEffectMode_==0
        &&screenSpaceReflectionActive(renderSettings_->screenSpaceReflection,renderSettings_->sceneLightingEnabled,qaIsolationMode_,true);
    surfaceRadiance_->beginSelectionFrame(static_cast<unsigned>(currentFrame_),permitted);
    if(!permitted){radianceSelectionFallback_="view-or-snapshot-policy";return;}
    try{
        if(!frame.selection||frame.selection->fastOffset()!=VkDeviceSize(frame.capacity)*20)
            frame.selection=std::make_unique<GpuPredicateDrawSelection>(*rhi_,frame.capacity,1,readBinaryFile(shaderDirectory_+"/predicate_draw_selection.comp.spv"));
        frame.selectionTicket=frame.selection->prepare(0,frame.commands->output(0),surfaceRadiance_->selectionBuffer(static_cast<unsigned>(currentFrame_)),frame.ticket->count,true);
        radianceSelectionFallback_.clear();
    }catch(const std::exception& error){radianceSelectionFallback_=error.what();
        RuntimeDiagnostics::instance().print("render","Radiance selection fallback: "+radianceSelectionFallback_);}
}
bool CharacterSceneRenderer::radianceSelectionActive(const RenderContext& context)const{
    return mainDepthSplitActive(context)&&conservativeFrames_.at(context.currentFrame).selectionTicket.has_value();
}
void CharacterSceneRenderer::recordRadianceSelection(rhi::ICommandRecorder& commands,const RenderContext& context){
    auto& frame=conservativeFrames_.at(context.currentFrame);
    if(!radianceSelectionActive(context))return;
    const auto& predicate=surfaceRadiance_->selectionBuffer(static_cast<unsigned>(context.currentFrame));
    const auto& source=frame.commands->output(0);
    commands.bufferBarrier({predicate.buffer,0,4,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    commands.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    frame.selection->record(commands,*frame.selectionTicket);
    commands.bufferBarrier({predicate.buffer,0,4,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_SHADER_WRITE_BIT});
    commands.bindComputePipeline(radianceCommitPipeline_);
    commands.bindComputeDescriptorSet(pipelineLayout_,descriptorSets_.at(bindlessTextures_?context.currentFrame:context.currentFrame*totalMaterialCount()));
    commands.dispatch(1,1,1);
    surfaceRadiance_->recordSelectionReady(commands,static_cast<unsigned>(context.currentFrame));
    const auto& output=frame.selection->output(0);
    commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_INDIRECT_COMMAND_READ_BIT});
}

rhi::RenderPassBeginDesc CharacterSceneRenderer::radianceValidationDescription(const RenderContext& context){
    auto& framebuffer=radianceValidationFrames_.at(context.currentFrame);auto& view=radianceValidationViews_.at(context.currentFrame);
    if(view!=context.sceneDepthView){if(framebuffer)rhi_->destroyFramebuffer(framebuffer);framebuffer=VK_NULL_HANDLE;view=VK_NULL_HANDLE;
        framebuffer=rhi_->createFramebuffer({radianceValidationPass_,{context.sceneDepthView},context.renderExtent.width,context.renderExtent.height});view=context.sceneDepthView;}
    return {radianceValidationPass_,framebuffer,context.renderExtent,{}};
}
