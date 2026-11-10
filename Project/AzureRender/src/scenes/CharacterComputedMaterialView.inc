// Optional aliases belong to the renderer and share each reference descriptor.
void CharacterSceneRenderer::initializeComputedMaterialView(const RenderContext& context){
    computedMaterialViewCapabilities_={};computedMaterialViewFrame_.fill(false);
    computedMaterialViewRequests_.store(0,std::memory_order_relaxed);
    computedMaterialViewSelections_.store(0,std::memory_order_relaxed);
    computedMaterialViewFallback_="build-disabled";
#if defined(AZURE_COMPUTED_MATERIAL_VIEW_PROGRAMS)
    const char* disabled=std::getenv("AZURE_DISABLE_COMPUTED_MATERIAL_VIEW");
    if(disabled&&std::string_view(disabled)=="1"){computedMaterialViewFallback_="disabled";return;}
    if(!context.depthTestEnabled){computedMaterialViewFallback_="depth-test-disabled";return;}
    auto& caps=computedMaterialViewCapabilities_;
    caps.quadControl=context.fragmentQuadControl;
    caps.bindless=bindlessTextures_;caps.singleSample=context.rasterizationSamples==VK_SAMPLE_COUNT_1_BIT;
    caps.referenceUv=materialUvNeeded_;caps.referenceCamera=referenceCameraPrepared_;caps.depthAvailable=true;
    if(context.fragmentQuadControl&&context.physicalDevice!=VK_NULL_HANDLE){
        VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(context.physicalDevice,&properties);caps.vendorId=properties.vendorID;
        const auto subgroup=queryMaterialUvGradientSupport(context.physicalDevice);
        caps.quadControl=(subgroup.stages&VK_SHADER_STAGE_FRAGMENT_BIT)!=0&&(subgroup.operations&VK_SUBGROUP_FEATURE_QUAD_BIT)!=0;
    }
    computedMaterialViewFallback_=computedMaterialViewEligible(caps,{1,0,0,1,2})?"ready":"unsupported-capability-or-policy";
#else
    (void)context;
#endif
}
void CharacterSceneRenderer::prepareComputedMaterialViewPipeline(VkPipeline original,const rhi::GraphicsPipelineDesc& desc){
    if(!desc.depthTest||(desc.depthCompare!=VK_COMPARE_OP_LESS&&desc.depthCompare!=VK_COMPARE_OP_LESS_OR_EQUAL&&desc.depthCompare!=VK_COMPARE_OP_EQUAL))return;
    if(!computedMaterialViewEligible(computedMaterialViewCapabilities_,{1,0,0,1,2}))return;
    const auto source=std::find_if(materialUvShaderPaths_.begin(),materialUvShaderPaths_.end(),[&](const auto& item){return item.first==desc.fragmentShader;});
    const auto path=source!=materialUvShaderPaths_.end()?computedMaterialViewProgram(source->second):std::nullopt;
    if(!path)return;
    VkShaderModule module=VK_NULL_HANDLE;VkPipeline pipeline=VK_NULL_HANDLE;
    try{
        if(computedMaterialViewPipelines_.size()>=512)throw std::runtime_error("Computed material view pipeline budget exceeded");
        module=rhi_->createShaderModule(readBinaryFile(*path));auto next=desc;next.fragmentShader=module;
        pipeline=rhi_->createGraphicsPipeline(next);rhi_->destroyShaderModule(module);module=VK_NULL_HANDLE;
        computedMaterialViewPipelines_.emplace_back(original,pipeline);
    }catch(const std::exception& error){
        if(module)rhi_->destroyShaderModule(module);if(pipeline)rhi_->destroyPipeline(pipeline);
        for(const auto& item:computedMaterialViewPipelines_)rhi_->destroyPipeline(item.second);
        computedMaterialViewPipelines_.clear();computedMaterialViewCapabilities_.quadControl=false;
        computedMaterialViewFallback_=error.what();
        RuntimeDiagnostics::instance().print("render","Computed material view reference fallback: "+computedMaterialViewFallback_);
    }
}
VkPipeline CharacterSceneRenderer::computedMaterialViewPipeline(VkPipeline original) const{
    if(computedMaterialViewPipelines_.empty())return original;
    computedMaterialViewRequests_.fetch_add(1,std::memory_order_relaxed);
    if(!computedMaterialViewFrame_.at(currentFrame_))return original;
    const auto found=std::find_if(computedMaterialViewPipelines_.begin(),computedMaterialViewPipelines_.end(),[&](const auto& item){return item.first==original;});
    if(found==computedMaterialViewPipelines_.end())return original;
    computedMaterialViewSelections_.fetch_add(1,std::memory_order_relaxed);return found->second;
}
void CharacterSceneRenderer::destroyComputedMaterialViewPipelines() noexcept{
    if(rhi_)for(const auto& item:computedMaterialViewPipelines_)rhi_->destroyPipeline(item.second);
    computedMaterialViewPipelines_.clear();computedMaterialViewCapabilities_={};computedMaterialViewFrame_.fill(false);
    computedMaterialViewRequests_.store(0,std::memory_order_relaxed);computedMaterialViewSelections_.store(0,std::memory_order_relaxed);
    computedMaterialViewFallback_.clear();
}
