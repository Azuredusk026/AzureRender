// Optional material shader variants share the renderer's layout and ownership.
void CharacterSceneRenderer::initializeOrdinaryOpaquePipelines(const RenderContext& context,
    const rhi::GraphicsPipelineDesc& color) {
    const auto eligible=[](const auto& asset){return std::any_of(asset.materials.begin(),asset.materials.end(),ordinaryOpaqueMaterial);};
    bool needed=eligible(asset_);for(const auto& resource:additionalResources_)needed|=eligible(resource->asset);
    if(!needed)return;
    try{
        for(unsigned facing=0;facing<3;++facing){
            auto desc=color;
            desc.frontFace=facing==1?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
            desc.cullMode=facing==2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
            desc.fragmentSpecialization={{0,VK_TRUE}};
            ordinaryOpaquePipelines_[facing]=rhi_->createGraphicsPipeline(desc);
        }
    }catch(...){destroyOrdinaryOpaquePipelines();throw;}
}
void CharacterSceneRenderer::destroyOrdinaryOpaquePipelines() noexcept {
    if(!rhi_)return;
    for(auto& pipeline:ordinaryOpaquePipelines_){
        if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;
    }
}

void CharacterSceneRenderer::initializeHairIntegralPipelines(const RenderContext& context,
    const rhi::GraphicsPipelineDesc& base) {
    bool opaque=false,blend=false;
    const auto inspect=[&](const auto& asset){for(const auto& material:asset.materials){
        if((material.materialFeatures&MaterialFeatureHairPixelIntegral)==0)continue;
        if(material.alphaMode==AssetAlphaMode::Blend)blend=true;else opaque=true;
    }};
    inspect(asset_);for(const auto& resource:additionalResources_)inspect(resource->asset);
    const bool multisample=context.rasterizationSamples==VK_SAMPLE_COUNT_4_BIT;
    if((!opaque&&!blend)||multisample)return;
    std::string suffix=objectShadowPrepared_?"_object":"";
    if(bindlessTextures_)suffix+="_bindless";
    VkShaderModule color=VK_NULL_HANDLE,snapshot=VK_NULL_HANDLE;
    try {
        color=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+"/mesh_integral"+suffix+".frag.spv"));
        if(opaque)snapshot=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+"/character_snapshot_integral"+suffix+".frag.spv"));
        for(unsigned kind=0;kind<2;++kind){
            if(kind==0?!opaque:!blend)continue;
            for(unsigned facing=0;facing<3;++facing){
                auto desc=base;desc.fragmentShader=color;
                desc.frontFace=facing==1?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing==2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                desc.alphaBlend=kind==1;desc.depthWrite=kind==0;
                hairIntegralPipelines_[kind*3+facing]=rhi_->createGraphicsPipeline(desc);
                if(kind==0){
                    desc.fragmentShader=snapshot;desc.renderPass=overlayDepthPass_;desc.colorAttachmentCount=2;
                    desc.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;desc.minSampleShading=0;
                    hairIntegralSnapshotPipelines_[facing]=rhi_->createGraphicsPipeline(desc);
                }
            }
        }
    }catch(...){
        if(color)rhi_->destroyShaderModule(color);
        if(snapshot)rhi_->destroyShaderModule(snapshot);
        destroyHairIntegralPipelines();throw;
    }
    if(color)rhi_->destroyShaderModule(color);
    if(snapshot)rhi_->destroyShaderModule(snapshot);
}
void CharacterSceneRenderer::destroyHairIntegralPipelines() noexcept {
    if(!rhi_)return;
    for(auto& pipeline:hairIntegralPipelines_){if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;}
    for(auto& pipeline:hairIntegralSnapshotPipelines_){if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;}
}
VkPipeline CharacterSceneRenderer::hairIntegralPipeline(const AssetMaterial& material,bool mirrored,bool snapshot) const {
    if(rasterizationSamples_==VK_SAMPLE_COUNT_4_BIT){
        if(snapshot)return material.doubleSided?overlayDepthDoubleSidedPipeline_:mirrored?overlayDepthMirroredPipeline_:overlayDepthPipeline_;
        if(material.alphaMode==AssetAlphaMode::Blend)return material.doubleSided?blendDoubleSidedPipeline_:mirrored?blendMirroredPipeline_:blendPipeline_;
        return material.doubleSided?opaqueDoubleSidedPipeline_:mirrored?opaqueMirroredPipeline_:opaquePipeline_;
    }
    const unsigned facing=material.doubleSided?2U:mirrored?1U:0U;
    const auto pipeline=snapshot?hairIntegralSnapshotPipelines_[facing]
        :hairIntegralPipelines_[(material.alphaMode==AssetAlphaMode::Blend?3U:0U)+facing];
    if(!pipeline)throw std::logic_error("Hair pixel integral pipeline is unavailable for the material");
    return pipeline;
}
