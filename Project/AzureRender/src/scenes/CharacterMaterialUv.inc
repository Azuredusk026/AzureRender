// The renderer owns reference variants beside its existing material pipelines.
VkShaderModule CharacterSceneRenderer::createUvShaderModule(const std::string& path){
    const auto module=rhi_->createShaderModule(readBinaryFile(path));
    try{materialUvShaderPaths_.emplace_back(module,path);}catch(...){rhi_->destroyShaderModule(module);throw;}
    return module;
}
void CharacterSceneRenderer::destroyUvShaderModule(VkShaderModule module){
    materialUvShaderPaths_.erase(std::remove_if(materialUvShaderPaths_.begin(),materialUvShaderPaths_.end(),
        [&](const auto& record){return record.first==module;}),materialUvShaderPaths_.end());
    rhi_->destroyShaderModule(module);
}
VkPipeline CharacterSceneRenderer::createMaterialUvGraphicsPipeline(const rhi::GraphicsPipelineDesc& desc){
    const auto original=rhi_->createGraphicsPipeline(desc);if(!materialUvNeeded_)return original;
    VkPipeline reference=original;VkShaderModule module=VK_NULL_HANDLE;
    try{
        bool selected=true;
        for(const auto& item:desc.fragmentSpecialization){
            if(item.id==0&&item.value)selected=false;
            if(item.id==1&&(item.value>=32||(materialUvClassesNeeded_&(1U<<item.value))==0))selected=false;
        }
        const auto source=std::find_if(materialUvShaderPaths_.begin(),materialUvShaderPaths_.end(),[&](const auto& item){return item.first==desc.fragmentShader;});
        const auto path=selected&&source!=materialUvShaderPaths_.end()?materialUvGradientProgram(source->second):std::nullopt;
        if(path){
            if(materialUvPipelines_.size()>=512)throw std::runtime_error("Material UV pipeline budget exceeded");
            module=rhi_->createShaderModule(readBinaryFile(*path));auto next=desc;next.fragmentShader=module;
            reference=rhi_->createGraphicsPipeline(next);rhi_->destroyShaderModule(module);module=VK_NULL_HANDLE;
        }
        materialUvPipelines_.emplace_back(original,reference);return original;
    }catch(...){if(module)rhi_->destroyShaderModule(module);if(reference!=original)rhi_->destroyPipeline(reference);rhi_->destroyPipeline(original);throw;}
}
VkPipeline CharacterSceneRenderer::materialUvPipeline(const AssetMaterial& material,VkPipeline original) const{
    if((material.materialFeatures&MaterialFeatureMirroredCoarseUv)==0)return original;
    const auto found=std::find_if(materialUvPipelines_.begin(),materialUvPipelines_.end(),[&](const auto& item){return item.first==original;});
    if(found==materialUvPipelines_.end())throw std::logic_error("Material UV pipeline is unavailable");
    return found->second;
}
void CharacterSceneRenderer::destroyMaterialUvPipelines() noexcept{
    if(rhi_)for(const auto& item:materialUvPipelines_)if(item.first!=item.second)rhi_->destroyPipeline(item.second);
    materialUvPipelines_.clear();materialUvShaderPaths_.clear();materialUvNeeded_=false;materialUvClassesNeeded_=0;
}
