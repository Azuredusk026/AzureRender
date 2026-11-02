// Optional material shader variants share the renderer's layout and ownership.
void CharacterSceneRenderer::prepareMaterialLightingPipelines() {
    lightingOrdinaryNeeded_=lightingBlendNeeded_=lightingFallbackNeeded_=lightingIntegralNeeded_=false;
    lightingClassesNeeded_={};
    const auto inspect=[&](const auto& asset){for(const auto& material:asset.materials){
        if(!extendedMaterialLighting(material))continue;
        const bool integral=(material.materialFeatures&MaterialFeatureHairPixelIntegral)!=0;
        if(integral&&rasterizationSamples_!=VK_SAMPLE_COUNT_4_BIT)lightingIntegralNeeded_=true;
        else if(material.alphaMode==AssetAlphaMode::Blend)lightingBlendNeeded_=true;
        else if(integral)lightingFallbackNeeded_=true;
        else if(ordinaryOpaqueMaterial(material))lightingOrdinaryNeeded_=true;
        else if(unsigned(material.materialClass)<lightingClassesNeeded_.size())
            lightingClassesNeeded_[unsigned(material.materialClass)]=true;
        else lightingFallbackNeeded_=true;
        if(material.materialFeatures&MaterialFeatureSceneTint)lightingBlendNeeded_=true;
    }};
    inspect(asset_);for(const auto& resource:additionalResources_)inspect(resource->asset);
}
VkShaderModule CharacterSceneRenderer::createMaterialShaderModule(const std::string& path) {
    const auto module=rhi_->createShaderModule(readBinaryFile(path));
    try{materialShaderVariants_.push_back({module,VK_NULL_HANDLE,path});}
    catch(...){rhi_->destroyShaderModule(module);throw;}
    return module;
}
void CharacterSceneRenderer::destroyMaterialShaderModule(VkShaderModule module) {
    const auto it=std::find_if(materialShaderVariants_.begin(),materialShaderVariants_.end(),
        [&](const auto& variant){return variant.scene==module;});
    if(it!=materialShaderVariants_.end()){
        if(it->lighting)rhi_->destroyShaderModule(it->lighting);
        materialShaderVariants_.erase(it);
    }
    rhi_->destroyShaderModule(module);
}
VkPipeline CharacterSceneRenderer::createMaterialGraphicsPipeline(const rhi::GraphicsPipelineDesc& desc) {
    auto it=std::find_if(materialShaderVariants_.begin(),materialShaderVariants_.end(),
        [&](const auto& variant){return variant.scene==desc.fragmentShader;});
    bool needed=desc.alphaBlend?lightingBlendNeeded_:lightingFallbackNeeded_;
    if(it!=materialShaderVariants_.end()&&integralMaterialProgram(it->path))
        needed=lightingIntegralNeeded_;
    else for(const auto& value:desc.fragmentSpecialization){
        if(value.id==0&&value.value)needed=lightingOrdinaryNeeded_;
        if(value.id==1)needed=value.value<lightingClassesNeeded_.size()&&lightingClassesNeeded_[value.value];
    }
    if(it==materialShaderVariants_.end()||!needed)return rhi_->createGraphicsPipeline(desc);
    if(!it->lighting)it->lighting=rhi_->createShaderModule(readBinaryFile(
        sceneFragmentVariant(it->path,false,false,false,true)));
    const auto scene=rhi_->createGraphicsPipeline(desc);
    VkPipeline lighting=VK_NULL_HANDLE;
    try{
        auto extended=desc;extended.fragmentShader=it->lighting;
        lighting=rhi_->createGraphicsPipeline(extended);
        materialLightingPipelines_.emplace_back(scene,lighting);
    }catch(...){if(lighting)rhi_->destroyPipeline(lighting);rhi_->destroyPipeline(scene);throw;}
    return scene;
}
VkPipeline CharacterSceneRenderer::materialLightingPipeline(const AssetMaterial& material,VkPipeline scene) const {
    if(!extendedMaterialLighting(material))return scene;
    const auto it=std::find_if(materialLightingPipelines_.begin(),materialLightingPipelines_.end(),
        [&](const auto& variant){return variant.first==scene;});
    if(it==materialLightingPipelines_.end())throw std::logic_error("Explicit material lighting pipeline is unavailable");
    return it->second;
}
void CharacterSceneRenderer::destroyMaterialLightingPipelines() noexcept {
    if(rhi_){
        for(const auto& variant:materialLightingPipelines_)rhi_->destroyPipeline(variant.second);
        for(const auto& variant:materialShaderVariants_){
            if(variant.lighting)rhi_->destroyShaderModule(variant.lighting);
            rhi_->destroyShaderModule(variant.scene);
        }
    }
    materialLightingPipelines_.clear();materialShaderVariants_.clear();
}
void CharacterSceneRenderer::initializeMaterialClassPipelines(const RenderContext& context,
    const rhi::GraphicsPipelineDesc& color,const rhi::GraphicsPipelineDesc& snapshot) {
    bool needed=false;
    std::array<bool,kMaterialClassVariantCount> classes{};
    std::array<std::uint32_t,kMaterialClassVariantCount> intersections{},combined{};
    intersections.fill(~std::uint32_t{0});
    materialClassFeatureSpecializations_={};
    const auto inspect=[&](const auto& asset){for(const auto& material:asset.materials){
        if(ordinaryOpaqueMaterial(material))needed=true;
        else if(material.alphaMode!=AssetAlphaMode::Blend){
            const auto index=unsigned(material.materialClass);
            if(index<classes.size()){
                classes[index]=true;
                intersections[index]&=material.materialFeatures;
                combined[index]|=material.materialFeatures;
            }
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
            ordinaryOpaquePipelines_[facing]=createMaterialGraphicsPipeline(desc);
            desc=snapshot;
            desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
            desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
            desc.fragmentSpecialization={{0,VK_TRUE}};
            ordinaryOpaqueSnapshotPipelines_[facing]=createMaterialGraphicsPipeline(desc);
        }
        for(unsigned index=0;index<classes.size();++index)if(classes[index]){
            const auto policy=fixedMaterialFeatures(intersections[index],combined[index]);
            materialClassFeatureSpecializations_[index]=policy;
            for(unsigned facing=0;facing<4;++facing){
                auto desc=color;
                desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                desc.fragmentSpecialization={{1,index},{2,policy.fixedMask},{3,policy.fixedValue}};
                materialClassPipelines_[index][facing]=createMaterialGraphicsPipeline(desc);
                desc=snapshot;
                desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                desc.fragmentSpecialization={{1,index},{2,policy.fixedMask},{3,policy.fixedValue}};
                materialClassSnapshotPipelines_[index][facing]=createMaterialGraphicsPipeline(desc);
            }
        }
    }catch(...){destroyMaterialClassPipelines();throw;}
}
void CharacterSceneRenderer::destroyMaterialClassPipelines() noexcept {
    materialClassFeatureSpecializations_={};
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
    if(!materialClassFeatureSpecializations_[index].matches(material.materialFeatures))return VK_NULL_HANDLE;
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
        color=createMaterialShaderModule(shaderDirectory_+sceneFragmentVariant("/mesh_integral"+suffix+".frag.spv",false,irradianceClipmapActive_,probeFieldActive_));
        if(opaque)snapshot=createMaterialShaderModule(shaderDirectory_+sceneFragmentVariant("/character_snapshot_integral"+suffix+".frag.spv",false,irradianceClipmapActive_,probeFieldActive_));
        for(unsigned kind=0;kind<2;++kind){
            if(kind==0?!opaque:!blend)continue;
            for(unsigned facing=0;facing<4;++facing){
                auto desc=base;desc.fragmentShader=color;
                desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                desc.alphaBlend=kind==1;desc.depthWrite=kind==0;
                hairIntegralPipelines_[kind*4+facing]=createMaterialGraphicsPipeline(desc);
                if(kind==0){
                    desc.fragmentShader=snapshot;desc.renderPass=overlayDepthPass_;desc.colorAttachmentCount=2;
                    desc.colorWriteMasks.assign(2,0xf);
                    desc.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;desc.minSampleShading=0;
                    hairIntegralSnapshotPipelines_[facing]=createMaterialGraphicsPipeline(desc);
                }
            }
        }
    }catch(...){
        if(color)destroyMaterialShaderModule(color);
        if(snapshot)destroyMaterialShaderModule(snapshot);
        destroyHairIntegralPipelines();throw;
    }
    if(color)destroyMaterialShaderModule(color);
    if(snapshot)destroyMaterialShaderModule(snapshot);
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
