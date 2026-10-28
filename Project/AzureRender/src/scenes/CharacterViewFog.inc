// Optional view-fog state has the same retirement boundary as the scene renderer.
struct CharacterSceneRenderer::ViewFogState {
    rhi::IRhi& backend;
    ViewFogGpu gpu;
    FogViewMatch match=FogViewMatch::Disabled;
    std::array<VkPipeline,2> surface{};
    std::array<std::array<std::array<VkPipeline,4>,2>,2> materials{};
    ViewFogState(rhi::IRhi& rhi,const ViewFogBinding& binding,unsigned frames)
        :backend(rhi),gpu(rhi,binding,FogVolumeData::load(std::filesystem::u8path(binding.asset)),frames){}
    ~ViewFogState(){releasePipelines();}
    void releasePipelines() noexcept {
        for(auto& pipeline:surface){if(pipeline)backend.destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;}
        for(auto& composite:materials)for(auto& color:composite)for(auto& pipeline:color){
            if(pipeline)backend.destroyPipeline(pipeline);pipeline=VK_NULL_HANDLE;
        }
    }
    void initializePipelines(const RenderContext& context,const rhi::GraphicsPipelineDesc& base,
        bool indexed,const LoadedAsset& primary,const std::vector<std::unique_ptr<AdditionalResource>>& additional){
        releasePipelines();
        VkShaderModule vertex=VK_NULL_HANDLE,fragment=VK_NULL_HANDLE,layered=VK_NULL_HANDLE;
        try{
            vertex=backend.createShaderModule(readBinaryFile(context.shaderDirectory+"/background.vert.spv"));
            fragment=backend.createShaderModule(readBinaryFile(context.shaderDirectory+
                (context.rasterizationSamples==VK_SAMPLE_COUNT_4_BIT?"/view_fog_surface_samples.frag.spv":"/view_fog_surface.frag.spv")));
            auto desc=base;desc.vertexShader=vertex;desc.fragmentShader=fragment;
            desc.vertexStride=0;desc.vertexBindings.clear();desc.vertexAttributes.clear();desc.vertexSpecialization={};
            desc.depthTest=desc.depthWrite=false;desc.cullMode=VK_CULL_MODE_NONE;desc.alphaBlend=true;desc.premultipliedAlpha=true;
            desc.colorWriteMasks.assign(desc.colorAttachmentCount,0);
            desc.colorWriteMasks[0]=VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT;
            for(unsigned pass=0;pass<2;++pass){
                desc.fragmentSpecialization={{0,pass}};
                desc.primaryColorBlend=pass?rhi::PrimaryColorBlend::Additive:rhi::PrimaryColorBlend::Multiply;
                surface[pass]=backend.createGraphicsPipeline(desc);
            }
            bool needed[2][2]{};
            const auto inspect=[&](const LoadedAsset& asset){for(const auto& material:asset.materials){
                const auto& p=material.unlitTextureLayers;
                if(p.present&&p.fogMode==UnlitTextureLayers::FogMode::Scene&&material.alphaMode==AssetAlphaMode::Blend)
                    needed[p.composite==UnlitTextureLayers::Composite::Additive][p.useVertexColor&&!asset.vertexColors.empty()]=true;
            }};
            inspect(primary);for(const auto& asset:additional)inspect(asset->asset);
            if(needed[0][0]||needed[0][1]||needed[1][0]||needed[1][1]){
                const auto file=sceneFragmentVariant(indexed?"/mesh_fog_bindless.frag.spv":"/mesh_fog.frag.spv",context.rasterizationSamples==VK_SAMPLE_COUNT_4_BIT,false,false);
                layered=backend.createShaderModule(readBinaryFile(context.shaderDirectory+file));
                for(unsigned additive=0;additive<2;++additive)for(unsigned color=0;color<2;++color)if(needed[additive][color]){
                    auto material=base;material.fragmentShader=layered;
                    material.alphaBlend=true;material.depthWrite=false;
                    material.primaryColorBlend=additive?rhi::PrimaryColorBlend::Additive:rhi::PrimaryColorBlend::Alpha;
                    material.vertexBindings[1].stride=color?sizeof(std::array<float,4>):0;
                    for(unsigned facing=0;facing<4;++facing){
                        material.frontFace=(facing&1U)?VK_FRONT_FACE_CLOCKWISE:VK_FRONT_FACE_COUNTER_CLOCKWISE;
                        material.cullMode=facing>=2||!context.faceCullingEnabled?VK_CULL_MODE_NONE:VK_CULL_MODE_BACK_BIT;
                        materials[additive][color][facing]=backend.createGraphicsPipeline(material);
                    }
                }
            }
        }catch(...){
            if(vertex)backend.destroyShaderModule(vertex);if(fragment)backend.destroyShaderModule(fragment);if(layered)backend.destroyShaderModule(layered);
            releasePipelines();throw;
        }
        backend.destroyShaderModule(vertex);backend.destroyShaderModule(fragment);if(layered)backend.destroyShaderModule(layered);
    }
    void update(unsigned frame,const CameraViewDescriptor& camera,std::array<std::uint32_t,2> extent){
        const auto current=gpu.updateFrame(frame,camera,extent);
        if(current!=match)RuntimeDiagnostics::instance().info("render","View fog: "+std::string(fogViewMatchName(current)));
        match=current;
    }
    VkPipeline materialPipeline(bool additive,bool color,unsigned facing) const{return materials[additive][color][facing];}
    void record(rhi::ICommandRecorder& commands,VkPipelineLayout layout,VkDescriptorSet descriptor,SceneSubmissionCounters* counters) const {
        commands.bindDescriptorSet(layout,descriptor);if(counters)++counters->descriptorSetBinds;
        for(const auto pipeline:surface){commands.bindPipeline(pipeline);commands.draw(3);if(counters){++counters->pipelineBinds;++counters->drawCalls;}}
    }
};
