void CharacterSceneRenderer::destroyConservativeOcclusion(){
    for(auto& framebuffer:radianceValidationFrames_){if(framebuffer)rhi_->destroyFramebuffer(framebuffer);framebuffer=VK_NULL_HANDLE;}
    radianceValidationViews_={};if(radianceValidationPass_)rhi_->destroyRenderPass(radianceValidationPass_);radianceValidationPass_=VK_NULL_HANDLE;

    for(auto& frame:conservativeFrames_){
        frame.commands.reset();frame.pyramid.reset();
        allocator_->destroyBuffer(frame.source);frame={};
    }
    mainDepthSplit_.reset();mainPassDescription_.reset();
    mainDepthSplitRecorded_={};
    conservativeClipMatrices_.clear();
    conservativeMeshes_.clear();conservativeOffsets_.clear();conservativeSlots_.clear();conservativeFallback_.clear();
}
void CharacterSceneRenderer::prepareConservativeOcclusion(){
    auto& frame=conservativeFrames_[currentFrame_];
    mainDepthSplitRecorded_[currentFrame_]=false;
    frame.lastZeroCommands=frame.commands?frame.commands->retiredZeroCommandCount(0):0;frame.ticket.reset();
    conservativeOffsets_.clear();conservativeSlots_.clear();
    if(!conservativeAllowed_||!renderSettings_->sceneLightingEnabled||gizmoActive_){conservativeFallback_="view-or-edit-policy";return;}
    if(!frame.failure.empty()){conservativeFallback_=frame.failure;return;}
    if(conservativeMeshes_.size()!=meshResourceCount_){
        conservativeMeshes_.assign(meshResourceCount_,false);const auto identity=scene::identityMatrix();
        for(std::size_t key=0;key<meshResourceCount_;++key){const auto& mesh=key==0?asset_:additionalResources_[key-1]->asset;
            bool valid=!mesh.hasSkin&&mesh.morphRanges.empty();
            const auto& matrices=loadedBindPoses_.at(key)->jointMatrices;
            for(const auto& matrix:matrices)valid&=matrix==identity;
            for(const auto& vertex:mesh.vertices){unsigned active=0;
                for(unsigned i=0;i<4;++i)if(vertex.weights[i]!=0){++active;valid&=vertex.weights[i]==1.F&&vertex.joints[i]<matrices.size();}
                valid&=active==1;
            }
            conservativeMeshes_[key]=valid;
        }
    }
    std::vector<bool> visible(sceneInstances_.size(),false);for(const auto* instance:visibleInstances_)visible.at(instance->sourceIndex)=true;
    std::vector<GpuOcclusionBounds> bounds;std::vector<VkDrawIndexedIndirectCommand> sources;
    for(const auto& instance:sceneInstances_){
        const auto& mesh=instance.meshKey==0?asset_:additionalResources_[instance.meshKey-1]->asset;
        conservativeOffsets_.push_back(conservativeSlots_.size());
        const auto& pose=instancePoses_.at(instance.sourceIndex);
        const bool candidate=visible[instance.sourceIndex]&&conservativeMeshes_[instance.meshKey]
            &&pose.pose==loadedBindPoses_[instance.meshKey]&&pose.morph[0]==0&&pose.morph[1]==0;
        for(const auto& primitive:mesh.primitives){
            const auto& material=mesh.materials.at(primitive.materialIndex);conservativeSlots_.push_back(UINT32_MAX);
            if(!candidate||!ordinaryOpaqueMaterial(material)||material.nprDiffuse.rimOrigin.present
                ||(material.showcasePlatform>.5F&&!renderSettings_->characterPresentation.platformEnabled)
                ||primitive.nodeIndex>=pose.pose->nodeWorldOpacities.size()||pose.pose->nodeWorldOpacities[primitive.nodeIndex]!=1.F)continue;
            GpuOcclusionBounds box;box.clipFromLocal=conservativeClipMatrices_.at(instance.sourceIndex);
            for(unsigned a=0;a<3;++a){const float margin=32.F*std::numeric_limits<float>::epsilon()
                    *(std::max(std::abs(mesh.boundsMin[a]),std::abs(mesh.boundsMax[a]))+1.F);
                box.minimum[a]=mesh.boundsMin[a]-margin;box.maximum[a]=mesh.boundsMax[a]+margin;}
            conservativeSlots_.back()=static_cast<std::uint32_t>(sources.size());bounds.push_back(box);
            sources.push_back({primitive.indexCount,1,primitive.firstIndex,0,instance.sourceIndex});
        }
    }
    // Every prepared command must belong to a fully qualified recording run.
    // Mixed-pose or mixed-opacity spans retain their original instanced draw.
    std::vector<bool> used(sources.size(),false);
    for(const auto& span:visibleSpansByMeshKey_){const auto& mesh=span[0]==0?asset_:additionalResources_[span[0]-1]->asset;
        for(std::size_t primitive=0;primitive<mesh.primitives.size();++primitive){bool complete=true;
            for(std::uint32_t i=0;i<span[2];++i)complete&=conservativeSlots_[conservativeOffsets_[span[1]+i]+primitive]!=UINT32_MAX;
            if(complete)for(std::uint32_t i=0;i<span[2];++i)used[conservativeSlots_[conservativeOffsets_[span[1]+i]+primitive]]=true;
        }
    }
    std::vector<std::uint32_t> remap(sources.size(),UINT32_MAX);std::size_t kept=0;
    for(std::size_t i=0;i<sources.size();++i)if(used[i]){remap[i]=static_cast<std::uint32_t>(kept);sources[kept]=sources[i];bounds[kept]=bounds[i];++kept;}
    sources.resize(kept);bounds.resize(kept);
    for(auto& slot:conservativeSlots_)if(slot!=UINT32_MAX)slot=remap[slot];
    if(sources.size()<256||sources.size()>65536){conservativeFallback_="static-work-range";return;}
    try{
        if(!mainDepthSplit_)mainDepthSplit_=std::make_unique<DepthPrepassSplit>(*rhi_,*mainPassDescription_);
        if(!frame.pyramid){
            unsigned levels=1;for(auto side=std::max(conservativeExtent_.width,conservativeExtent_.height);side>1;side/=2)++levels;
            frame.pyramid=std::make_unique<DepthPyramidGpu>(*rhi_,conservativeExtent_,levels,1,readBinaryFile(shaderDirectory_+"/depth_pyramid.comp.spv"));
        }
        if(sources.size()>frame.capacity){
            std::uint32_t capacity=256;while(capacity<sources.size())capacity*=2;
            auto commands=std::make_unique<GpuConservativeOcclusion>(*rhi_,capacity,1,readBinaryFile(shaderDirectory_+"/conservative_occlusion.comp.spv"));
            auto source=allocator_->createBuffer(VkDeviceSize(capacity)*20,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
            if(!source.mapped){allocator_->destroyBuffer(source);throw std::runtime_error("Occlusion source is not mapped");}
            frame.commands=std::move(commands);allocator_->destroyBuffer(frame.source);frame.source=source;frame.capacity=capacity;
        }
        std::memcpy(frame.source.mapped,sources.data(),sources.size()*20);allocator_->flush(frame.source,0,sources.size()*20);
        frame.ticket=frame.commands->prepare(0,frame.source,bounds,frame.pyramid->output(0),
            {0,0,float(conservativeExtent_.width),float(conservativeExtent_.height),0,1});conservativeFallback_.clear();
    }catch(const std::exception& error){frame.failure=error.what();conservativeFallback_=frame.failure;
        RuntimeDiagnostics::instance().print("render","Conservative occlusion fallback: "+frame.failure);}
}
bool CharacterSceneRenderer::mainDepthSplitActive(const RenderContext& context)const{
    if(!mainDepthSplit_||!conservativeFrames_.at(context.currentFrame).ticket||!context.depthTestEnabled
        ||context.rasterizationSamples!=VK_SAMPLE_COUNT_1_BIT||context.scenePassDescription!=mainPassDescription_
        ||!context.sceneFramebuffer||!context.sceneDepthImage||!context.sceneDepthView)return false;
    const auto colors=mainPassDescription_->attachments.size()-1;
    if(colors>context.sceneColorImages.size())return false;
    for(std::size_t i=0;i<colors;++i)if(!context.sceneColorImages[i])return false;
    return context.renderExtent.width==conservativeExtent_.width&&context.renderExtent.height==conservativeExtent_.height;
}
rhi::RenderPassBeginDesc CharacterSceneRenderer::conservativeDepthPass(const RenderContext& context)const{
    auto result=mainPassDescription(context);result.renderPass=mainDepthSplit_->depthPass();return result;
}
void CharacterSceneRenderer::recordConservativeHierarchy(rhi::ICommandRecorder& commands,const RenderContext& context){
    conservativeFrames_.at(context.currentFrame).pyramid->record(commands,0,context.sceneDepthView,DepthReduction::Maximum,{},VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
}
void CharacterSceneRenderer::recordConservativeOcclusion(const RenderContext& context,const SceneInstanceSnapshot& snapshot){
    if(!mainDepthSplitActive(context))return;
    mainDepthSplitRecorded_[context.currentFrame]=true;
    auto& frame=conservativeFrames_.at(context.currentFrame);auto& commands=*context.commands;
    commands.beginRenderPass(conservativeDepthPass(context));
    recordDepthCoverage(context,snapshot,commands,DepthCoverageMode::MainPrefill,context.submissionCounters);commands.endRenderPass();
    commands.imageBarrier({context.sceneDepthImage,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,1,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
    recordConservativeHierarchy(commands,context);
    commands.bufferBarrier({frame.source.buffer,0,frame.source.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    frame.commands->record(commands,*frame.ticket);const auto& output=frame.commands->output(0);
    commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_INDIRECT_COMMAND_READ_BIT});
    std::vector<VkImage> colors(context.sceneColorImages.begin(),context.sceneColorImages.begin()+mainPassDescription_->attachments.size()-1);
    mainDepthSplit_->beforeColor(commands,context.sceneDepthImage,colors);
}
