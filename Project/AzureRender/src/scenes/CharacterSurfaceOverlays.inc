// Surface overlays add radiance after opaque ownership is established.
void CharacterSceneRenderer::initializeSurfaceOverlayPipelines(
    const rhi::GraphicsPipelineDesc& color,const rhi::GraphicsPipelineDesc& snapshot) {
    std::array<bool,2> needed{};
    const auto inspect=[&](const LoadedAsset& mesh){for(const auto& material:mesh.materials)
        if(opaqueSurfaceOverlay(material))needed[material.unlitTextureLayers.useVertexColor&&!mesh.vertexColors.empty()]=true;};
    inspect(asset_);for(const auto& resource:additionalResources_)inspect(resource->asset);
    for(unsigned pass=0;pass<2;++pass)for(unsigned colored=0;colored<2;++colored)if(needed[colored]) {
        auto desc=pass?snapshot:color;
        desc.alphaBlend=true;desc.depthWrite=false;desc.depthCompare=VK_COMPARE_OP_LESS_OR_EQUAL;
        desc.primaryColorBlend=rhi::PrimaryColorBlend::Additive;
        desc.vertexBindings[1].stride=colored?sizeof(std::array<float,4>):0;
        desc.colorWriteMasks.assign(desc.colorAttachmentCount,0);
        desc.colorWriteMasks[0]=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT;
        auto& pipelines=pass?surfaceOverlaySnapshotPipelines_[colored]:surfaceOverlayPipelines_[colored];
        for(unsigned facing=0;facing<4;++facing) {
            auto variant=desc;
            variant.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
            variant.cullMode=facing>=2||color.cullMode==VK_CULL_MODE_NONE?VK_CULL_MODE_NONE:VK_CULL_MODE_BACK_BIT;
            pipelines[facing]=rhi_->createGraphicsPipeline(variant);
        }
    }
}
void CharacterSceneRenderer::recordSurfaceOverlays(const RenderContext& context,
    const SceneInstanceSnapshot& snapshot,rhi::ICommandRecorder& commands,bool snapshotColor,
    SceneSubmissionCounters* counters) {
    if(!surfaceOverlayPipelines_[0][0]&&!surfaceOverlayPipelines_[1][0])return;
    for(const auto& span:snapshot.visibleSpans) {
        const auto key=span[0];const auto& mesh=key==0?asset_:additionalResources_[key-1]->asset;
        if(std::none_of(mesh.materials.begin(),mesh.materials.end(),opaqueSurfaceOverlay))continue;
        commands.bindVertexBuffer(snapshot.buffers.vertices.at(key),0);
        commands.bindIndexBuffer(snapshot.buffers.indices.at(key),0);
        const auto textureBase=key==0?kSharedTextureSlots:additionalResources_[key-1]->textureBase;
        const auto materialBase=key==0?0:additionalResources_[key-1]->globalMaterialBase;
        for(const auto& primitive:mesh.primitives)if(opaqueSurfaceOverlay(mesh.materials[primitive.materialIndex]))
            drawPrimitive(commands,mesh,primitive,primitive.firstIndex,span[2],span[1],textureBase,
                materialBase,counters,&snapshot,snapshotColor);
    }
    (void)context;
}
