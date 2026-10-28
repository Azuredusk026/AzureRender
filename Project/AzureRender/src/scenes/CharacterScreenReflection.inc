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
        if(!context.renderSettings||!context.renderSettings->screenSpaceReflection.enabled)return;
        reflectionPreparation_="queue-without-compute";if(!context.computeShaders)return;
        reflectionPreparation_="depth-only-snapshot";if(reflectionSurfaceFormat_!=VK_FORMAT_R32G32B32A32_SFLOAT)return;
        reflectionPreparation_="resource-limits";if(!ScreenReflectionGpu::supported(*rhi_,context.renderExtent,kMaxFramesInFlight))return;
        screenReflections_=std::make_unique<ScreenReflectionGpu>(*rhi_,context.renderExtent,static_cast<unsigned>(kMaxFramesInFlight),ScreenReflectionShaders::load(shaderDirectory_));
        reflectionPreparation_="compute";
    }catch(...){allocator_->destroyBuffer(staging);destroyScreenReflectionResources();throw;}
}
void CharacterSceneRenderer::destroyScreenReflectionResources(){
    screenReflections_.reset();reflectionDescriptorPublished_.fill(false);reflectionActive_.fill(false);
    if(reflectionFallbackView_)rhi_->destroyImageView(reflectionFallbackView_);reflectionFallbackView_=VK_NULL_HANDLE;
    allocator_->destroyImage(reflectionFallback_);
}
void CharacterSceneRenderer::publishScreenReflectionOutput(unsigned frame){
    if(reflectionDescriptorPublished_.at(frame))return;
    const auto output=screenReflections_->output(frame);const auto count=bindlessTextures_?1U:static_cast<unsigned>(totalMaterialCount());
    for(unsigned index=0;index<count;++index)rhi_->writeDescriptorImage({descriptorSets_.at(frame*count+index),37,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        output.view,VK_NULL_HANDLE,output.layout});
    reflectionDescriptorPublished_[frame]=true;
}
