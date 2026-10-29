// Optional material shader variants share the renderer's layout and ownership.
void CharacterSceneRenderer::initializeMaterialClassPipelines(const RenderContext& context,
    const rhi::GraphicsPipelineDesc& color,const rhi::GraphicsPipelineDesc& snapshot) {
    bool needed=false;
    std::array<bool,kMaterialClassVariantCount> classes{};
    const auto inspect=[&](const auto& asset){for(const auto& material:asset.materials){
        if(ordinaryOpaqueMaterial(material))needed=true;
        else if(material.alphaMode!=AssetAlphaMode::Blend){
            const auto index=unsigned(material.materialClass);
            if(index<classes.size())classes[index]=true;
        }
    }};
    inspect(asset_);for(const auto& resource:additionalResources_)inspect(resource->asset);
    if(!needed&&std::none_of(classes.begin(),classes.end(),[](bool value){return value;}))return;
    try{
        if(needed)for(unsigned facing=0;facing<4;++facing){
            auto desc=color;
            desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
            desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
            desc.fragmentSpecialization={{0,VK_TRUE}};
            ordinaryOpaquePipelines_[facing]=rhi_->createGraphicsPipeline(desc);
            desc=snapshot;
            desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
            desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
            desc.fragmentSpecialization={{0,VK_TRUE}};
            ordinaryOpaqueSnapshotPipelines_[facing]=rhi_->createGraphicsPipeline(desc);
        }
        for(unsigned index=0;index<classes.size();++index)if(classes[index]){
            for(unsigned facing=0;facing<4;++facing){
                auto desc=color;
                desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                desc.fragmentSpecialization={{1,index}};
                materialClassPipelines_[index][facing]=rhi_->createGraphicsPipeline(desc);
                desc=snapshot;
                desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                desc.fragmentSpecialization={{1,index}};
                materialClassSnapshotPipelines_[index][facing]=rhi_->createGraphicsPipeline(desc);
            }
        }
    }catch(...){destroyMaterialClassPipelines();throw;}
}
void CharacterSceneRenderer::destroyMaterialClassPipelines() noexcept {
    if(!rhi_)return;
    for(auto* variants:{&materialClassPipelines_,&materialClassSnapshotPipelines_})
        for(auto& facing:*variants)for(auto& pipeline:facing){
            if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;
        }
    for(auto& pipeline:ordinaryOpaquePipelines_){
        if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;
    }
    for(auto& pipeline:ordinaryOpaqueSnapshotPipelines_){
        if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;
    }
}

VkPipeline CharacterSceneRenderer::materialClassPipeline(const AssetMaterial& material,bool mirrored,bool snapshot) const noexcept {
    const auto index=unsigned(material.materialClass);
    if(material.alphaMode==AssetAlphaMode::Blend||index>=kMaterialClassVariantCount)return VK_NULL_HANDLE;
    return (snapshot?materialClassSnapshotPipelines_:materialClassPipelines_)[index][surfaceFacingIndex(material.doubleSided,mirrored)];
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
        color=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+sceneFragmentVariant("/mesh_integral"+suffix+".frag.spv",false,irradianceClipmapActive_,probeFieldActive_)));
        if(opaque)snapshot=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+sceneFragmentVariant("/character_snapshot_integral"+suffix+".frag.spv",false,irradianceClipmapActive_,probeFieldActive_)));
        for(unsigned kind=0;kind<2;++kind){
            if(kind==0?!opaque:!blend)continue;
            for(unsigned facing=0;facing<4;++facing){
                auto desc=base;desc.fragmentShader=color;
                desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                desc.alphaBlend=kind==1;desc.depthWrite=kind==0;
                hairIntegralPipelines_[kind*4+facing]=rhi_->createGraphicsPipeline(desc);
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
        if(snapshot)return overlayDepthPipelines_[surfaceFacingIndex(material.doubleSided,mirrored)];
        if(material.alphaMode==AssetAlphaMode::Blend)return blendPipelines_[surfaceFacingIndex(material.doubleSided,mirrored)];
        return opaquePipelines_[surfaceFacingIndex(material.doubleSided,mirrored)];
    }
    const unsigned facing=surfaceFacingIndex(material.doubleSided,mirrored);
    const auto pipeline=snapshot?hairIntegralSnapshotPipelines_[facing]
        :hairIntegralPipelines_[(material.alphaMode==AssetAlphaMode::Blend?4U:0U)+facing];
    if(!pipeline)throw std::logic_error("Hair pixel integral pipeline is unavailable for the material");
    return pipeline;
}
