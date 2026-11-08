// Optional material shader variants share the renderer's layout and ownership.
void CharacterSceneRenderer::prepareMaterialLightingPipelines() {
    lightingOrdinaryNeeded_=lightingBlendNeeded_=lightingFallbackNeeded_=lightingIntegralNeeded_=false;
    lightingClassesNeeded_={};
    independentCoverageNeeded_=false;
    materialDepthPoliciesNeeded_={};
    const auto inspect=[&](const auto& asset){for(const auto& material:asset.materials){
        independentCoverageNeeded_|=material.coverageTexture.present;
        if(material.unlitTextureLayers.present&&material.unlitTextureLayers.depthCompare==UnlitTextureLayers::DepthCompare::LessOrEqual)
            materialDepthPoliciesNeeded_[1]=true;
        if(material.blendDepth.present)materialDepthPoliciesNeeded_[(material.blendDepth.write?2U:0U)
            |(material.blendDepth.compare==AssetMaterial::BlendDepth::Compare::LessOrEqual?1U:0U)]=true;
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
        if(it->coverage)rhi_->destroyShaderModule(it->coverage);
        if(it->lightingCoverage)rhi_->destroyShaderModule(it->lightingCoverage);
        for(auto extra:it->defaultIsolation)if(extra)rhi_->destroyShaderModule(extra);
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
    if(it==materialShaderVariants_.end()||(!needed&&!independentCoverageNeeded_))return createMaterialDepthPipeline(desc);
    const auto scene=createMaterialDepthPipeline(desc);
    std::array<VkPipeline,3> variants{};
    try{
        for(unsigned policy=1;policy<4;++policy){
            if((policy&1U)&&!needed)continue;
            if((policy&2U)&&!independentCoverageNeeded_)continue;
            auto& module=policy==1?it->lighting:policy==2?it->coverage:it->lightingCoverage;
            auto path=it->path;const auto cachedEnd=path.rfind(".frag.spv");
            const auto cached=cachedEnd!=std::string::npos&&cachedEnd>=15&&path.compare(cachedEnd-15,15,"_radiance_stats")==0?cachedEnd-15:
                cachedEnd!=std::string::npos&&cachedEnd>=9&&path.compare(cachedEnd-9,9,"_radiance")==0?cachedEnd-9:std::string::npos;
            if(cached!=std::string::npos&&cachedEnd!=std::string::npos)path.erase(cached,cachedEnd-cached);
            if(!module)module=rhi_->createShaderModule(readBinaryFile(
                sceneFragmentVariant(path,false,false,false,(policy&1U)!=0,(policy&2U)!=0)));
            auto extended=desc;extended.fragmentShader=module;
            if(cached!=std::string::npos&&extended.colorAttachmentCount>3)
                for(std::size_t i=2;i<extended.colorWriteMasks.size();++i)extended.colorWriteMasks[i]=0;
            variants[policy-1]=createMaterialDepthPipeline(extended);
        }
        materialLightingPipelines_.emplace_back(scene,variants);
    }catch(...){for(auto pipeline:variants)if(pipeline)rhi_->destroyPipeline(pipeline);rhi_->destroyPipeline(scene);throw;}
    return scene;
}
VkPipeline CharacterSceneRenderer::createMaterialDepthPipeline(const rhi::GraphicsPipelineDesc& desc) {
    const auto scene=rhi_->createGraphicsPipeline(desc);
    prepareDefaultIsolationPipeline(scene,desc);
    if(!desc.alphaBlend||!desc.depthTest
        ||desc.depthWrite||desc.depthCompare!=VK_COMPARE_OP_LESS)return scene;
    std::array<VkPipeline,3> variants{};
    try {
        for(unsigned policy=1;policy<4;++policy)if(materialDepthPoliciesNeeded_[policy]) {
            auto variant=desc;variant.depthWrite=(policy&2U)!=0;
            variant.depthCompare=(policy&1U)?VK_COMPARE_OP_LESS_OR_EQUAL:VK_COMPARE_OP_LESS;
            variants[policy-1]=rhi_->createGraphicsPipeline(variant);
        }
        if(std::any_of(variants.begin(),variants.end(),[](auto value){return value!=VK_NULL_HANDLE;}))
            materialDepthPipelines_.emplace_back(scene,variants);
    }catch(...) {
        for(auto pipeline:variants)if(pipeline)rhi_->destroyPipeline(pipeline);
        rhi_->destroyPipeline(scene);throw;
    }
    return scene;
}
void CharacterSceneRenderer::prepareDefaultIsolationPipeline(VkPipeline scene,const rhi::GraphicsPipelineDesc& desc) {
#if defined(AZURE_DEFAULT_ISOLATION_VARIANTS)
    // Preparation runs before submission; the original pipeline remains caller-owned.
    if(!defaultIsolationFallback_.empty()||desc.renderPass!=defaultIsolationRenderPass_
        ||desc.rasterizationSamples!=VK_SAMPLE_COUNT_1_BIT||desc.alphaBlend)return;
    if(std::none_of(desc.fragmentSpecialization.begin(),desc.fragmentSpecialization.end(),
        [](const auto& value){return value.id==0&&value.value==VK_TRUE;}))return;
    for(auto& source:materialShaderVariants_){
        const std::array<VkShaderModule,4> modules{source.scene,source.lighting,source.coverage,source.lightingCoverage};
        const auto found=std::find(modules.begin(),modules.end(),desc.fragmentShader);
        if(found==modules.end()||source.path.find("_early.frag.spv")==std::string::npos)continue;
        const auto policy=static_cast<unsigned>(found-modules.begin());
        VkPipeline pipeline=VK_NULL_HANDLE;
        try{
            if(defaultIsolationPipelines_.size()>=16)throw std::runtime_error("Default isolation pipeline limit exceeded");
            auto& module=source.defaultIsolation[policy];
            if(!module){
                auto file=sceneFragmentVariant(source.path,false,false,false,(policy&1U)!=0,(policy&2U)!=0);
                file.insert(file.size()-9,"_isolation0");
                module=rhi_->createShaderModule(readBinaryFile(file));
            }
            auto isolated=desc;isolated.fragmentShader=module;isolated.fragmentSpecialization.push_back({4,0});
            pipeline=rhi_->createGraphicsPipeline(isolated);
            defaultIsolationPipelines_.emplace_back(scene,pipeline);
        }catch(const std::exception& error){
            if(pipeline)rhi_->destroyPipeline(pipeline);
            for(const auto& entry:defaultIsolationPipelines_)rhi_->destroyPipeline(entry.second);
            defaultIsolationPipelines_.clear();defaultIsolationFallback_=error.what();
            RuntimeDiagnostics::instance().print("render","Default isolation preparation fallback: "+defaultIsolationFallback_);
        }
        return;
    }
#else
    (void)scene;(void)desc;
#endif
}
VkPipeline CharacterSceneRenderer::createCoverageGraphicsPipeline(const rhi::GraphicsPipelineDesc& desc,const std::string& path) {
    const auto module=rhi_->createShaderModule(readBinaryFile(sceneFragmentVariant(path,false,false,false,false,true)));
    try{auto variant=desc;variant.fragmentShader=module;const auto pipeline=rhi_->createGraphicsPipeline(variant);
        rhi_->destroyShaderModule(module);return pipeline;
    }catch(...){rhi_->destroyShaderModule(module);throw;}
}
VkPipeline CharacterSceneRenderer::materialLightingPipeline(const AssetMaterial& material,VkPipeline scene) const {
    const auto policy=(extendedMaterialLighting(material)?1U:0U)|(material.coverageTexture.present?2U:0U);
    if(policy) {
        const auto it=std::find_if(materialLightingPipelines_.begin(),materialLightingPipelines_.end(),
            [&](const auto& variant){return variant.first==scene;});
        if(it==materialLightingPipelines_.end()||!it->second[policy-1])throw std::logic_error("Explicit material lighting/coverage pipeline is unavailable");
        scene=it->second[policy-1];
    }
    const auto depthPolicy=material.blendDepth.present
        ?(material.blendDepth.write?2U:0U)|(material.blendDepth.compare==AssetMaterial::BlendDepth::Compare::LessOrEqual?1U:0U)
        :(material.unlitTextureLayers.present&&material.unlitTextureLayers.depthCompare==UnlitTextureLayers::DepthCompare::LessOrEqual?1U:0U);
    if(depthPolicy) {
        const auto it=std::find_if(materialDepthPipelines_.begin(),materialDepthPipelines_.end(),
            [&](const auto& variant){return variant.first==scene;});
        if(it==materialDepthPipelines_.end()||!it->second[depthPolicy-1])throw std::logic_error("Explicit surface depth pipeline is unavailable");
        scene=it->second[depthPolicy-1];
    }
    if(qaIsolationMode_==0){
        const auto found=std::find_if(defaultIsolationPipelines_.begin(),defaultIsolationPipelines_.end(),
            [&](const auto& entry){return entry.first==scene;});
        if(found!=defaultIsolationPipelines_.end())return found->second;
    }
    return scene;
}
void CharacterSceneRenderer::destroyMaterialLightingPipelines() noexcept {
    if(rhi_){
        for(const auto& entry:defaultIsolationPipelines_)rhi_->destroyPipeline(entry.second);
        for(auto* group:{&surfaceOverlayPipelines_,&surfaceOverlaySnapshotPipelines_})
            for(auto& facing:*group)for(auto& pipeline:facing) {
                if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;
            }
        for(const auto& variant:materialDepthPipelines_)for(auto pipeline:variant.second)if(pipeline)rhi_->destroyPipeline(pipeline);
        for(const auto& variant:materialLightingPipelines_)for(auto pipeline:variant.second)if(pipeline)rhi_->destroyPipeline(pipeline);
        for(const auto& variant:materialShaderVariants_){
            if(variant.lighting)rhi_->destroyShaderModule(variant.lighting);
            if(variant.coverage)rhi_->destroyShaderModule(variant.coverage);
            if(variant.lightingCoverage)rhi_->destroyShaderModule(variant.lightingCoverage);
            for(auto module:variant.defaultIsolation)if(module)rhi_->destroyShaderModule(module);
            rhi_->destroyShaderModule(variant.scene);
        }
    }
    materialDepthPipelines_.clear();materialLightingPipelines_.clear();materialShaderVariants_.clear();
    defaultIsolationPipelines_.clear();defaultIsolationFallback_.clear();defaultIsolationRenderPass_=VK_NULL_HANDLE;
}
void CharacterSceneRenderer::initializeMaterialClassPipelines(const RenderContext& context,
    const rhi::GraphicsPipelineDesc& color,const rhi::GraphicsPipelineDesc& snapshot,const std::string& vertexPath) {
    VkShaderModule identityVertex=VK_NULL_HANDLE;
    VkShaderModule validationModule=VK_NULL_HANDLE;
    bool needed=false;
    std::array<bool,kMaterialClassVariantCount> classes{};
    std::array<bool,kMaterialClassVariantCount> coveredClasses{};
    std::array<std::uint32_t,kMaterialClassVariantCount> intersections{},combined{};
    intersections.fill(~std::uint32_t{0});
    materialClassFeatureSpecializations_={};
    const auto inspect=[&](const auto& asset){for(const auto& material:asset.materials){
        if(ordinaryOpaqueMaterial(material))needed=true;
        else if(material.alphaMode!=AssetAlphaMode::Blend){
            const auto index=unsigned(material.materialClass);
            if(index<classes.size()){
                classes[index]=true;
                coveredClasses[index]=coveredClasses[index]||fullyCoveredClassMaterial(material,1.F);
                intersections[index]&=material.materialFeatures;
                combined[index]|=material.materialFeatures;
            }
        }
    }};
    inspect(asset_);for(const auto& resource:additionalResources_)inspect(resource->asset);
    if(!needed&&std::none_of(classes.begin(),classes.end(),[](bool value){return value;}))return;
    try{
        const auto earlyModule=[&](VkShaderModule module){
            const auto found=std::find_if(materialShaderVariants_.begin(),materialShaderVariants_.end(),
                [&](const auto& variant){return variant.scene==module;});
            if(found==materialShaderVariants_.end())throw std::logic_error("Ordinary material program identity is unavailable");
            const auto file=sceneFragmentVariant(found->path,false,false,false,false,false,true);
            return createMaterialShaderModule(file);
        };
        const bool early=(needed||std::any_of(coveredClasses.begin(),coveredClasses.end(),[](bool value){return value;}))
            &&rasterizationSamples_==VK_SAMPLE_COUNT_1_BIT;
        const auto earlyColorModule=early?earlyModule(color.fragmentShader):VK_NULL_HANDLE;
        const auto earlySnapshotModule=early?earlyModule(snapshot.fragmentShader):VK_NULL_HANDLE;
        VkShaderModule cachedColor=VK_NULL_HANDLE,cachedSnapshot=VK_NULL_HANDLE;
        if(needed&&early&&surfaceRadiance_){
            auto vertex=vertexPath;const auto at=vertex.rfind(".vert.spv");
            if(at==std::string::npos)throw std::logic_error("Surface radiance identity requires a mesh vertex program");
            vertex.insert(at,"_radiance");identityVertex=rhi_->createShaderModule(readBinaryFile(vertex));
            const auto cachedModule=[&](VkShaderModule module,bool fast=false){
                const auto found=std::find_if(materialShaderVariants_.begin(),materialShaderVariants_.end(),[&](const auto& v){return v.scene==module;});
                if(found==materialShaderVariants_.end())throw std::logic_error("Surface radiance material program identity is unavailable");
                auto file=found->path;file.insert(file.rfind(".frag.spv"),fast?"_radiance_fast":kSurfaceRadianceDiagnostics?"_radiance_stats":"_radiance");return createMaterialShaderModule(file);
            };
            cachedColor=cachedModule(earlyColorModule,kSurfaceRadianceFrameSelection);cachedSnapshot=cachedModule(earlySnapshotModule);
            if(kSurfaceRadianceFrameSelection)validationModule=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+"/surface_radiance_validation.frag.spv"));
            if(kSurfaceRadianceFrameSelection){auto commit=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+"/surface_radiance_draw_commit.comp.spv"));
                try{radianceCommitPipeline_=rhi_->createComputePipeline({commit,pipelineLayout_});}catch(...){rhi_->destroyShaderModule(commit);throw;}
                rhi_->destroyShaderModule(commit);
                if(radianceDepthCopyPass_){
                    auto vertex=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+"/surface_radiance_depth_copy.vert.spv"));
                    VkShaderModule fragment=VK_NULL_HANDLE;
                    try{fragment=rhi_->createShaderModule(readBinaryFile(shaderDirectory_+"/surface_radiance_depth_copy.frag.spv"));
                        rhi::GraphicsPipelineDesc copy;copy.vertexShader=vertex;copy.fragmentShader=fragment;copy.layout=pipelineLayout_;
                        copy.renderPass=radianceDepthCopyPass_;copy.colorAttachmentCount=0;copy.cullMode=VK_CULL_MODE_NONE;
                        copy.depthWrite=true;copy.depthCompare=VK_COMPARE_OP_ALWAYS;
                        radianceDepthCopyPipeline_=rhi_->createGraphicsPipeline(copy);
                    }catch(...){if(fragment)rhi_->destroyShaderModule(fragment);rhi_->destroyShaderModule(vertex);throw;}
                    rhi_->destroyShaderModule(fragment);rhi_->destroyShaderModule(vertex);
                }}

        }
        if(needed)for(unsigned facing=0;facing<4;++facing){
            auto desc=color;
            desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
            desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
            desc.fragmentSpecialization={{0,VK_TRUE}};
            ordinaryOpaquePipelines_[facing]=createMaterialGraphicsPipeline(desc);
            if(early){auto covered=desc;covered.fragmentShader=earlyColorModule;
                if(identityVertex&&!kSurfaceRadianceFrameSelection){covered.vertexShader=identityVertex;covered.fragmentShader=cachedColor;}
                ordinaryEarlyDepthPipelines_[facing]=createMaterialGraphicsPipeline(covered);
                if(identityVertex&&kSurfaceRadianceFrameSelection){
                    auto fast=covered;fast.vertexShader=identityVertex;fast.fragmentShader=cachedColor;
                    ordinaryRadiancePipelines_[facing]=rhi_->createGraphicsPipeline(fast);
                    if(radianceValidationPass_){auto validation=fast;validation.fragmentShader=validationModule;validation.depthWrite=true;
                        validation.renderPass=radianceValidationPass_;validation.colorAttachmentCount=0;validation.colorWriteMasks.clear();
                        ordinaryRadianceValidationPipelines_[facing]=rhi_->createGraphicsPipeline(validation);}
                }
            }
            desc=snapshot;
            desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
            desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
            desc.fragmentSpecialization={{0,VK_TRUE}};
            ordinaryOpaqueSnapshotPipelines_[facing]=createMaterialGraphicsPipeline(desc);
            if(early){auto covered=desc;covered.fragmentShader=earlySnapshotModule;
                if(identityVertex){covered.vertexShader=identityVertex;covered.fragmentShader=cachedSnapshot;
                    for(std::size_t i=2;i<covered.colorWriteMasks.size();++i)covered.colorWriteMasks[i]=0xf;}
                ordinaryEarlyDepthSnapshotPipelines_[facing]=createMaterialGraphicsPipeline(covered);}
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
                if(early&&coveredClasses[index]){auto covered=desc;covered.fragmentShader=earlyColorModule;
                    materialClassEarlyDepthPipelines_[index][facing]=createMaterialGraphicsPipeline(covered);}
                desc=snapshot;
                desc.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                desc.cullMode=facing>=2?VK_CULL_MODE_NONE:context.faceCullingEnabled?VK_CULL_MODE_BACK_BIT:VK_CULL_MODE_NONE;
                desc.fragmentSpecialization={{1,index},{2,policy.fixedMask},{3,policy.fixedValue}};
                materialClassSnapshotPipelines_[index][facing]=createMaterialGraphicsPipeline(desc);
                if(early&&coveredClasses[index]){auto covered=desc;covered.fragmentShader=earlySnapshotModule;
                    materialClassEarlyDepthSnapshotPipelines_[index][facing]=createMaterialGraphicsPipeline(covered);}
            }
        }
    }catch(...){if(validationModule)rhi_->destroyShaderModule(validationModule);if(identityVertex)rhi_->destroyShaderModule(identityVertex);destroyMaterialClassPipelines();throw;}
    if(validationModule)rhi_->destroyShaderModule(validationModule);
    if(identityVertex)rhi_->destroyShaderModule(identityVertex);
}
void CharacterSceneRenderer::destroyMaterialClassPipelines() noexcept {
    materialClassFeatureSpecializations_={};
    if(!rhi_)return;
    if(radianceDepthCopyPipeline_)rhi_->destroyPipeline(radianceDepthCopyPipeline_);radianceDepthCopyPipeline_=VK_NULL_HANDLE;
    if(radianceCommitPipeline_)rhi_->destroyPipeline(radianceCommitPipeline_);radianceCommitPipeline_=VK_NULL_HANDLE;
    for(auto* variants:{&materialClassPipelines_,&materialClassSnapshotPipelines_,&materialClassEarlyDepthPipelines_,&materialClassEarlyDepthSnapshotPipelines_})
        for(auto& facing:*variants)for(auto& pipeline:facing){
            if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;
        }
    for(auto* pipelines:{&ordinaryOpaquePipelines_,&ordinaryOpaqueSnapshotPipelines_,&ordinaryEarlyDepthPipelines_,&ordinaryEarlyDepthSnapshotPipelines_,&ordinaryRadiancePipelines_,&ordinaryRadianceValidationPipelines_})
        for(auto& pipeline:*pipelines){if(pipeline)rhi_->destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;}
}

VkPipeline CharacterSceneRenderer::ordinaryOpaquePipeline(const AssetMaterial& material,bool mirrored,bool snapshot,float nodeOpacity) const noexcept {
    const auto facing=surfaceFacingIndex(material.doubleSided,mirrored);
    if(rasterizationSamples_==VK_SAMPLE_COUNT_1_BIT&&fullyCoveredOrdinaryMaterial(material,nodeOpacity))
        return (snapshot?ordinaryEarlyDepthSnapshotPipelines_:ordinaryEarlyDepthPipelines_)[facing];
    return (snapshot?ordinaryOpaqueSnapshotPipelines_:ordinaryOpaquePipelines_)[facing];
}

VkPipeline CharacterSceneRenderer::materialClassPipeline(const AssetMaterial& material,bool mirrored,bool snapshot,float nodeOpacity) const noexcept {
    const auto index=unsigned(material.materialClass);
    if(material.alphaMode==AssetAlphaMode::Blend||index>=kMaterialClassVariantCount)return VK_NULL_HANDLE;
    if(!materialClassFeatureSpecializations_[index].matches(material.materialFeatures))return VK_NULL_HANDLE;
    if(rasterizationSamples_==VK_SAMPLE_COUNT_1_BIT&&fullyCoveredClassMaterial(material,nodeOpacity)){
        const auto early=(snapshot?materialClassEarlyDepthSnapshotPipelines_:materialClassEarlyDepthPipelines_)[index][surfaceFacingIndex(material.doubleSided,mirrored)];
        if(early)return early;
    }
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
                    desc.fragmentShader=snapshot;desc.renderPass=overlayDepthPass_;desc.colorAttachmentCount=surfaceRadiance_?6U:2U;
                    desc.colorWriteMasks.assign(desc.colorAttachmentCount,0);desc.colorWriteMasks[0]=0xf;desc.colorWriteMasks[1]=0xf;
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
