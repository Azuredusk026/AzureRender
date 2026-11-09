void CharacterSceneRenderer::initializeShadowHistory(const RenderContext& context){
    if(!shadowHistoryCapable_)return;
    try{
        shadowHistory_=std::make_unique<ShadowHistoryCache>(*rhi_,context.renderExtent,2,1,readBinaryFile(shaderDirectory_+"/shadow_history_clear.comp.spv"));
        // The frame inputs grow only after the corresponding completion fence.
        for(auto& buffer:shadowHistoryInputs_){buffer=allocator_->createBuffer(104,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);std::memset(buffer.mapped,0,104);allocator_->flush(buffer,0,104);}
    }catch(const std::exception& error){destroyShadowHistory();shadowHistoryCapable_=false;
        RuntimeDiagnostics::instance().print("render",std::string("Shadow history allocation fallback: ")+error.what());}
}
void CharacterSceneRenderer::destroyShadowHistory()noexcept{
    cancelShadowHistory();shadowHistory_.reset();shadowHistoryCasters_.clear();shadowHistorySubmittedSlot_.reset();shadowHistorySubmission_=0;
    if(allocator_)for(auto& buffer:shadowHistoryInputs_){allocator_->destroyBuffer(buffer);buffer={};}
    shadowHistoryStatisticsValid_={};
}
void CharacterSceneRenderer::cancelShadowHistory()noexcept{
    shadowHistoryView_.invalidate();shadowHistoryClear_.reset();shadowHistoryRecorded_=false;
}
void CharacterSceneRenderer::writeShadowHistoryDescriptors(VkDescriptorSet set,unsigned slot){
    if(!shadowHistory_)return;
    for(auto entry:{std::pair<unsigned,const rhi::GpuBuffer*>{45,&shadowHistory_->buffer(slot^1U,0)},
            {46,&shadowHistory_->buffer(slot,0)},{49,&shadowHistoryInputs_.at(slot)}})
        rhi_->writeDescriptorBuffer({set,entry.first,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,entry.second->buffer,entry.second->size});
}
void CharacterSceneRenderer::prepareShadowHistory(){
    shadowHistoryRecorded_=false;shadowHistoryClear_.reset();if(!shadowHistory_)return;
    const auto slot=static_cast<unsigned>(currentFrame_);auto& buffer=shadowHistoryInputs_.at(slot);
    allocator_->invalidate(buffer,0,buffer.size);
    const auto* old=static_cast<const std::uint32_t*>(buffer.mapped);
    if(shadowHistoryStatisticsValid_[slot]){shadowHistoryRequests_+=old[20];shadowHistoryEligible_+=old[21];shadowHistoryHits_+=old[22];shadowHistoryStatisticsValid_[slot]=false;}
    const auto count=sceneInstances_.size(),bytes=96+8*std::max<std::size_t>(1,count);
    if(bytes>buffer.size){
        auto replacement=allocator_->createBuffer(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);allocator_->destroyBuffer(buffer);buffer=replacement;
        const auto first=bindlessTextures_?slot:slot*totalMaterialCount();
        for(std::size_t i=0;i<(bindlessTextures_?1:totalMaterialCount());++i)writeShadowHistoryDescriptors(descriptorSets_.at(first+i),slot);
    }
    std::vector<float> identity;identity.reserve(256+count*17);
    const auto append=[&](const auto& values){identity.insert(identity.end(),values.begin(),values.end());};
    append(viewMatrix_);append(projectionMatrix_);append(samplingProjectionMatrix_);
    for(const auto& matrix:cascadeLightViewProjections_)append(matrix);
    const auto* uniform=static_cast<const float*>(uniformBuffers_.at(slot).mapped);identity.insert(identity.end(),uniform,uniform+41*4);
    append(gizmoTranslation_);append(gizmoRotation_);append(gizmoScale_);identity.push_back(gizmoActive_?1.F:0.F);identity.push_back(float(selectedPrimitiveIndex_));
    std::array<ShadowHistoryDirtyRect,4> dirty{};std::vector<ShadowHistoryCaster> casters(count);
    std::vector<std::array<std::uint32_t,2>> ranges(count,{UINT32_MAX,0});ShadowHistoryPrimitiveRanges allocator;
    const VkExtent2D tile{shadowCascadeResolution_,shadowCascadeResolution_};
    const auto dirtyBounds=[&](const scene::AxisAlignedBounds& bounds){for(unsigned cascade=0;cascade<4;++cascade)
        dirty[cascade].include(shadowHistoryProjectBounds(bounds.minimum,bounds.maximum,cascadeLightViewProjections_[cascade],tile));};
    for(const auto& instance:sceneInstances_){
        const auto& mesh=instance.meshKey==0?asset_:additionalResources_.at(instance.meshKey-1)->asset;
        const auto& pose=instancePoses_.at(instance.sourceIndex);
        const bool animated=std::any_of(animationFrames_.begin(),animationFrames_.end(),[&](const auto& frame){return frame.node==instance.nodeId;});
        const bool dynamic=mesh.hasSkin||!mesh.animations.empty()||!mesh.morphRanges.empty()||animated
            ||pose.morph[0]!=0.F||pose.morph[1]!=0.F||std::signbit(pose.morph[0])||std::signbit(pose.morph[1]);
        casters.at(instance.sourceIndex)={instance.worldBounds,dynamic};
        if(dynamic)dirtyBounds(instance.worldBounds);
        else{
            identity.push_back(float(instance.meshKey));append(instance.model);
            if(mesh.indices.size()<=UINT32_MAX){const auto indices=static_cast<std::uint32_t>(mesh.indices.size());const auto base=allocator.reserve(indices);
                if(base!=UINT32_MAX)ranges.at(instance.sourceIndex)={base,indices};}
        }
    }
    for(const auto& previous:shadowHistoryCasters_)if(previous.dynamic)dirtyBounds(previous.bounds);
    shadowHistoryCasters_=std::move(casters);
    const bool unchanged=shadowHistoryView_.begin(identity,outlineMaskViewExtent_,temporalSceneGeneration_);
    const bool reuse=unchanged&&shadowHistorySubmittedSlot_&&*shadowHistorySubmittedSlot_==(slot^1U);
    auto* words=static_cast<std::uint32_t*>(buffer.mapped);std::memset(words,0,static_cast<std::size_t>(bytes));
    words[0]=outlineMaskViewExtent_.width;words[1]=outlineMaskViewExtent_.height;words[2]=reuse?1U:0U;words[3]=static_cast<std::uint32_t>(count);
    for(unsigned cascade=0;cascade<4;++cascade){const auto& rect=dirty[cascade];const std::array<float,4> values{float(rect.left),float(rect.top),float(rect.right),float(rect.bottom)};std::memcpy(words+4+cascade*4,values.data(),16);}
    if(count)std::memcpy(words+24,ranges.data(),count*8);allocator_->flush(buffer,0,bytes);
    shadowHistoryClear_=shadowHistory_->prepareClear(slot,0);
}
void CharacterSceneRenderer::recordShadowHistoryReady(rhi::ICommandRecorder& commands,unsigned slot){
    if(!shadowHistory_||!shadowHistoryClear_)return;
    const auto& previous=shadowHistory_->buffer(slot^1U,0);const auto& inputs=shadowHistoryInputs_.at(slot);
    commands.bufferBarrier({previous.buffer,0,previous.size,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    commands.bufferBarrier({inputs.buffer,0,inputs.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
    shadowHistory_->recordClear(commands,*shadowHistoryClear_);shadowHistoryClear_.reset();shadowHistoryRecorded_=true;
}
void CharacterSceneRenderer::commitShadowHistory(std::uint64_t submission)noexcept{
    if(shadowHistory_&&shadowHistoryRecorded_&&submission>shadowHistorySubmission_){
        shadowHistoryView_.commit();shadowHistorySubmittedSlot_=static_cast<unsigned>(currentFrame_);shadowHistorySubmission_=submission;shadowHistoryStatisticsValid_.at(currentFrame_)=true;
    }else shadowHistoryView_.invalidate();
    shadowHistoryRecorded_=false;shadowHistoryClear_.reset();
}
void CharacterSceneRenderer::recordShadowHistoryReadback(rhi::ICommandRecorder& commands,unsigned slot){
    if(!shadowHistory_)return;const auto& inputs=shadowHistoryInputs_.at(slot);
    commands.bufferBarrier({inputs.buffer,0,inputs.size,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
}
