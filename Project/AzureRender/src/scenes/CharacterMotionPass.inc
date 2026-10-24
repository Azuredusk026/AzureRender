// Included after the renderer's primitive helpers. The public motion pass owns
// GPU submission state; this adapter supplies the asset's frozen draw inputs.
std::uint32_t CharacterSceneRenderer::materialFeaturesForFrame(const AssetMaterial& material) const noexcept {
    constexpr auto customCoverage=MaterialFeatureOverlay|MaterialFeatureSceneTint|MaterialFeatureSceneRefraction
        |MaterialFeatureMultiplyComposite|MaterialFeatureEyeRadialSurface;
    const bool temporalCoverage=(material.alphaMode==AssetAlphaMode::Blend||material.alphaMode==AssetAlphaMode::Mask)&&material.materialClass!=AssetMaterialClass::Overlay
        &&(material.materialFeatures&customCoverage)==0
        &&motionPass_&&renderSettings_&&renderSettings_->antiAliasing==3&&state_.temporalFrame;
    return materialSamplingFeatures(material.materialFeatures,temporalMaterialSamplesActive()
        &&material.alphaMode!=AssetAlphaMode::Blend&&material.materialClass!=AssetMaterialClass::Overlay,temporalCoverage);
}
bool CharacterSceneRenderer::temporalMaterialSamplesActive() const noexcept {
    return motionPass_&&renderSettings_&&renderSettings_->antiAliasing==3&&state_.temporalFrame&&state_.temporalFrame->previous.has_value();
}
void CharacterSceneRenderer::prepareTemporalFrame(const SceneFrameData& frame){
    TemporalFrameInfo temporal;
    temporal.width=std::max(frame.swapchainWidth,1U);temporal.height=std::max(frame.swapchainHeight,1U);
    temporal.sceneGeneration=temporalSceneGeneration_;temporal.cameraCut=static_cast<std::uint64_t>(frame.camera.source);
    temporal.projection=projectionMatrix_;temporal.viewProjection=multiply(projectionMatrix_,viewMatrix_);
    temporal.exposureScale=std::exp2(renderSettings_->grade.exposureEv);
    const auto finite=[](const auto& matrix){return std::all_of(matrix.begin(),matrix.end(),[](float value){return std::isfinite(value);});};
    if(frame.swapchainWidth&&frame.swapchainHeight&&finite(temporal.projection)&&finite(temporal.viewProjection))temporalView_.prepare(temporal);
    else temporalView_.clear();
    state_.temporalFrame=temporalView_.pending()?&*temporalView_.pending():nullptr;
    samplingJitter_={};samplingProjectionMatrix_=projectionMatrix_;
    if(motionPass_&&renderSettings_->antiAliasing==3&&state_.temporalFrame&&state_.temporalFrame->previous){
        samplingJitter_=temporalJitterPixels(state_.temporalFrame->sampleIndex);
        const float x=2*samplingJitter_[0]/temporal.width,y=2*samplingJitter_[1]/temporal.height;
        for(unsigned column=0;column<4;++column){samplingProjectionMatrix_[column*4]+=x*projectionMatrix_[column*4+3];
            samplingProjectionMatrix_[column*4+1]+=y*projectionMatrix_[column*4+3];}
    }
}
void CharacterSceneRenderer::initializeMotion(const RenderContext& context){
    motionTarget_=context.motionOutput;
    if(totalMaterialCount()>UINT32_MAX)throw std::length_error("Motion material count exceeds descriptor addressing");
    if(motionTarget_)motionPass_=std::make_unique<MotionVectorPass>(*rhi_,*allocator_,*motionTarget_,shaderDirectory_,
        static_cast<std::uint32_t>(totalMaterialCount()),static_cast<std::uint32_t>(kMaxFramesInFlight),context.faceCullingEnabled,context.depthTestEnabled);
}
void CharacterSceneRenderer::onFrameSubmitted(std::uint64_t submission) noexcept{
    if(temporalView_.commit(submission))motionPoses_.commit(submission);
    state_.temporalFrame=temporalView_.committed()?&*temporalView_.committed():nullptr;
}
void CharacterSceneRenderer::onFrameAbandoned() noexcept{
    temporalView_.cancel();motionPoses_.cancel();
    state_.temporalFrame=temporalView_.committed()?&*temporalView_.committed():nullptr;
}
void CharacterSceneRenderer::prepareMotion(){
    motionDraws_.clear();motionPreviousValidObjects_=0;if(!motionPass_||!instanceSnapshot_)return;
    const auto& snapshot=*instanceSnapshot_;
    std::vector<MotionPoseState> poses;poses.reserve(snapshot.instances.size());
    std::map<std::pair<std::string,std::uint32_t>,std::uint32_t> replicas;
    for(const auto& instance:snapshot.instances){
        const auto& mesh=instance.meshKey==0?asset_:additionalResources_[instance.meshKey-1]->asset;
        const auto& pose=instancePoses_.at(instance.sourceIndex);
        MotionPoseState previous;previous.identity.node=instance.nodeId.empty()?"$default":instance.nodeId;
        previous.identity.geometry=instance.meshKey;
        previous.identity.replica=replicas[{previous.identity.node,instance.meshKey}]++;
        previous.model=instance.model;previous.joints=pose.pose.jointMatrices;
        previous.morphWeights=flattenMorphGpuWeights(mesh,pose.pose);previous.legacyMorph=pose.morph;
        previous.vertexCount=static_cast<std::uint32_t>(mesh.vertices.size());previous.morphTableBase=morphGpuMeshes_.at(instance.meshKey).tableBase;
        poses.push_back(std::move(previous));
    }
    MotionPoseView view;view.cameraPosition=cameraPosition_;view.outlinePixels=snapshot.settings.outline.silhouetteWidthPixels;
    if(snapshot.gizmo.active&&snapshot.gizmo.selectedPrimitive>=0){view.selectedPrimitive=static_cast<unsigned>(snapshot.gizmo.selectedPrimitive);
        view.gizmo=primitiveGizmoTransform(snapshot.gizmo,view.selectedPrimitive);}
    motionPoses_.stage(std::move(poses),view);
    const auto* temporal=state_.temporalFrame;
    const auto packed=motionPoses_.pack(temporal&&temporal->previous?temporal->previous->viewProjection:motionIdentityMatrix(),
        temporal?temporal->previousSubmission:0,128ULL*1024*1024);
    motionPreviousValidObjects_=packed.validObjects;
    MotionVectorInputs inputs;inputs.instances=&instanceBuffers_.at(currentFrame_);inputs.joints=&jointBuffers_.at(currentFrame_);
    inputs.weights=&morphWeightBuffers_.at(currentFrame_);inputs.targets=&morphTargetBuffer_;
    inputs.outline=outlineProjectionEnabled_?&outlineProjection_->buffer():inputs.instances;
    const auto addMaterials=[&](const std::vector<GpuMaterial>& materials){for(const auto& material:materials)inputs.colors.push_back(material.baseColor);};
    addMaterials(gpuMaterials_);for(const auto& resource:additionalResources_)addMaterials(resource->gpuMaterials);
    if(outlineProjectionEnabled_)inputs.masks=outlineProjection_->masks();else inputs.masks=inputs.colors;
    inputs.coverage={outlineMasks_[currentFrame_].view,outlineMasks_[currentFrame_].sampler};
    if(!inputs.coverage.view||!inputs.coverage.sampler)inputs.coverage=inputs.colors.front();
    motionPass_->prepare(static_cast<unsigned>(currentFrame_),packed,inputs);
    // Match the colour order: opaque geometry, visible shells, transparent
    // surfaces. Transparent pixels explicitly reject accumulated history.
    for(unsigned stage=0;stage<3;++stage)for(const auto source:snapshot.visibleIndices){
        const auto& instance=snapshot.instances[source];const auto key=instance.meshKey;
        const auto& mesh=key==0?asset_:additionalResources_[key-1]->asset;
        const auto materialBase=key==0?0:additionalResources_[key-1]->globalMaterialBase;
        for(std::size_t primitiveIndex=0;primitiveIndex<mesh.primitives.size();++primitiveIndex){
            const auto& primitive=mesh.primitives[primitiveIndex];const auto& m=mesh.materials.at(primitive.materialIndex);
            const bool reactive=m.alphaMode==AssetAlphaMode::Blend||m.materialClass==AssetMaterialClass::Overlay;
            const bool shell=stage==1;
            if(shell){if(reactive||m.showcasePlatform>.5F||!((m.materialFeatures&MaterialFeatureSilhouetteOutline)!=0
                ||(!snapshot.settings.sceneLightingEnabled&&snapshot.settings.silhouetteOutlineEnabled)))continue;
            }else if(reactive!=(stage==2))continue;
            forEachPrimitiveOpacityRun(snapshot.nodeWorldOpacities,primitive.nodeIndex,source,1,[&](const PrimitiveOpacityRun& run){
                if(run.opacity==0)return;
                MotionVectorDraw draw;draw.vertices=key==0?vertexBuffer_.buffer:additionalResources_[key-1]->vertexBuffer.buffer;
                draw.indices=key==0?indexBuffer_.buffer:additionalResources_[key-1]->indexBuffer.buffer;
                if(primitive.firstIndex>UINT32_MAX||primitive.indexCount>UINT32_MAX)throw std::length_error("Motion geometry exceeds indexed draw range");
                draw.firstIndex=static_cast<std::uint32_t>(primitive.firstIndex);draw.indexCount=static_cast<std::uint32_t>(primitive.indexCount);draw.instance=source;
                draw.mirrored=primitiveMirroredTransform(instance.model,snapshot.gizmo,primitiveIndex);draw.doubleSided=m.doubleSided;draw.shell=shell;
                auto& push=draw.push;push.alphaCutoff=m.alphaCutoff;push.material=static_cast<std::uint32_t>(materialBase+primitive.materialIndex);
                push.primitive=static_cast<unsigned>(primitiveIndex);push.gizmo=primitiveGizmoTransform(snapshot.gizmo,primitiveIndex);
                push.flags=static_cast<unsigned>(m.alphaMode)|(reactive?16U:0U)|(outlineProjectionEnabled_?256U:0U)|(shell?512U:0U);
                push.camera={cameraPosition_[0],cameraPosition_[1],cameraPosition_[2],0};
                const auto tile=shell?snapshot.buffers.outlineMaskTiles.at(source):UINT32_MAX;std::memcpy(&push.camera[3],&tile,4);
                push.detail={m.featureParameters[0],0,0,0};const auto frameFeatures=materialFeaturesForFrame(m);std::memcpy(&push.detail[1],&frameFeatures,4);
                if(temporal){push.detail[2]=2*samplingJitter_[0]/temporal->current.width;push.detail[3]=2*samplingJitter_[1]/temporal->current.height;}
                push.outline={view.outlinePixels,float(temporal?temporal->current.width:1),float(temporal?temporal->current.height:1),run.opacity};
                motionDraws_.push_back(draw);
            });
        }
    }
}
