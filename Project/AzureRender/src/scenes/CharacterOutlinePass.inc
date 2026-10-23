// Included by CharacterSceneRenderer.cpp after its shared primitive helpers.
// Both shell passes consume the same frozen instance and deformation inputs.
namespace {
struct OutlinePushConstants {
    float samplingScale=1,reserved=0;
    std::uint32_t tile=UINT32_MAX;
    float opacity=1;
    std::array<float,16> gizmoTransform{};
};
static_assert(sizeof(OutlinePushConstants)==80);
static_assert(offsetof(OutlinePushConstants,tile)==8);
static_assert(offsetof(OutlinePushConstants,gizmoTransform)==16);
}
void CharacterSceneRenderer::recordOutlineShells(const RenderContext& context,
    const SceneInstanceSnapshot& snapshot,rhi::ICommandRecorder& commands,
    bool depthShell,SceneSubmissionCounters* counters) {
    if(depthShell&&!outlineProjectionEnabled_)return;
    const auto& settings=snapshot.settings;
    // Both callers bind the global indexed set before their ordinary draws.
    // Fixed descriptors remain material-local and are selected below.
    const auto eligible=[&](const AssetMaterial& material) {
        if(material.showcasePlatform>.5F||material.materialClass==AssetMaterialClass::Overlay
            ||material.alphaMode==AssetAlphaMode::Blend)return false;
        return depthShell?(material.outlineSurface.present&&material.outlineSurface.depthPrepass)
            :((material.materialFeatures&MaterialFeatureSilhouetteOutline)!=0
                ||(!settings.sceneLightingEnabled&&settings.silhouetteOutlineEnabled));
    };
    for(const auto& span:snapshot.visibleSpans) {
        const auto key=span[0],first=span[1],count=span[2];
        const auto& mesh=key==0?asset_:additionalResources_[key-1]->asset;
        if(std::none_of(mesh.primitives.begin(),mesh.primitives.end(),[&](const AssetPrimitive& primitive) {
            return eligible(mesh.materials[primitive.materialIndex]);
        }))continue;
        const auto materialBase=key==0?0:additionalResources_[key-1]->globalMaterialBase;
        const auto textureBase=key==0?kSharedTextureSlots:additionalResources_[key-1]->textureBase;
        commands.bindVertexBuffer(snapshot.buffers.vertices.at(key),0);
        commands.bindIndexBuffer(snapshot.buffers.indices.at(key),0);
        for(const auto& primitive:mesh.primitives) {
            const auto& material=mesh.materials[primitive.materialIndex];
            if(!eligible(material))continue;
            const auto primitiveIndex=static_cast<std::size_t>(&primitive-mesh.primitives.data());
            const bool mirrored=primitiveMirroredTransform(snapshot.instances.at(first).model,snapshot.gizmo,primitiveIndex);
            commands.bindPipeline(depthShell?(mirrored?outlineDepthMirroredPipeline_:outlineDepthPipeline_)
                :(mirrored?outlineMirroredPipeline_:outlinePipeline_));
            const auto identity=static_cast<std::uint32_t>(materialBase+primitive.materialIndex);
            if(!bindlessTextures_) {
                commands.bindDescriptorSet(pipelineLayout_,descriptorSets_[context.currentFrame*totalMaterialCount()+identity]);
                if(counters)++counters->descriptorSetBinds;
            }
            if(outlineProjectionEnabled_) {
                struct CoveragePush {float cutoff;std::uint32_t mode;};
                const CoveragePush coverage{material.alphaCutoff,static_cast<std::uint32_t>(material.alphaMode)};
                commands.pushConstants(pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,&coverage,sizeof(coverage));
                const std::array<std::uint32_t,2> indices{textureBase+primitive.materialIndex*kMaterialTextureSlots,identity};
                commands.pushConstants(pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,120,indices.data(),sizeof(indices));
                if(counters)counters->pushConstantUpdates+=2;
            }
            OutlinePushConstants morph{};morph.samplingScale=settings.antiAliasing==2?2.F:1.F;
            morph.gizmoTransform=primitiveGizmoTransform(snapshot.gizmo,primitiveIndex);
            commands.pushConstants(pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                sizeof(MaterialPushConstants),&morph,sizeof(morph));
            if(counters)++counters->pushConstantUpdates;
            forEachPrimitiveOpacityRun(snapshot.nodeWorldOpacities,primitive.nodeIndex,first,count,[&](const PrimitiveOpacityRun& run) {
                if(run.opacity==0)return;
                for(std::uint32_t index=0;index<run.instanceCount;) {
                    const auto source=run.firstInstance+index;
                    const auto tile=depthShell?std::numeric_limits<std::uint32_t>::max():snapshot.buffers.outlineMaskTiles.at(source);
                    std::uint32_t batch=1;
                    if(depthShell||tile==std::numeric_limits<std::uint32_t>::max())
                        while(index+batch<run.instanceCount&&(depthShell||snapshot.buffers.outlineMaskTiles.at(source+batch)==tile))++batch;
                    struct MaskPush {std::uint32_t tile;float opacity;};
                    const MaskPush coverage{tile,run.opacity};
                    commands.pushConstants(pipelineLayout_,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
                        sizeof(MaterialPushConstants)+offsetof(OutlinePushConstants,tile),&coverage,sizeof(coverage));
                    commands.drawIndexed(primitive.indexCount,primitive.firstIndex,batch,source);
                    if(counters){++counters->drawCalls;++counters->pushConstantUpdates;}
                    index+=batch;
                }
            });
        }
    }
}
