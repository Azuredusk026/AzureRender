// Included in GltfLoader.cpp's private loader namespace.
void loadUnlitTextureSurface(const tinygltf::Material& source,const tinygltf::Model& model,
    const nlohmann::json& profile,AssetMaterial& result,int& baseImage,int& secondImage,int& coverageImage) {
    const auto fail=[](const char* field){throw std::invalid_argument(std::string("Invalid unlit texture layers: ")+field);};
    constexpr std::uint32_t allowed=MaterialFeatureUnlitSurface|MaterialFeatureUnlitTextureLayers;
    if(result.materialProfileVersion!=2||(result.materialFeatures&allowed)!=allowed
        ||(result.materialFeatures&~allowed)||result.alphaMode!=AssetAlphaMode::Blend
        ||(result.materialClass!=AssetMaterialClass::Generic&&result.materialClass!=AssetMaterialClass::Emissive))fail("material capability ownership");
    for(const auto& field:profile.items())if(field.key()!="schemaVersion"&&field.key()!="class"
        &&field.key()!="features"&&field.key()!="surface")fail("profile field ownership");
    const auto& pbr=source.pbrMetallicRoughness;
    if(!source.extensions.empty()||source.normalTexture.index>=0||source.occlusionTexture.index>=0
        ||source.emissiveTexture.index>=0||pbr.metallicRoughnessTexture.index>=0)fail("standard material channel ownership");
    for(double value:pbr.baseColorFactor)if(value!=1)fail("baseColorFactor ownership");
    for(double value:source.emissiveFactor)if(value!=0)fail("emissiveFactor ownership");
    auto layers=azurerender::parseUnlitTextureLayers(profile.at("surface"),model.textures.size());
    const auto& standard=pbr.baseColorTexture;
    if(standard.index>=0&&(standard.index!=layers.layers[0].texture||standard.texCoord!=0||!standard.extensions.empty()))fail("baseColorTexture ownership");
    const auto sampler=[&](int index,azurerender::TextureImportSettings& target) {
        target.usage="albedo";target.colorSpace="srgb";target.anisotropy=1;target.mipBias=0;
        target.filter="linear";target.wrapU=target.wrapV="repeat";target.mipPolicy="generate";
        target.mipFilter=azurerender::TextureImportSettings::MipFilter::Linear;
        if(index<0)return;
        if(static_cast<std::size_t>(index)>=model.samplers.size())fail("sampler index");
        const auto& input=model.samplers[static_cast<std::size_t>(index)];
        if(!input.extensions.empty())fail("sampler extension");
        const auto wrap=[&](int value)->std::string {
            if(value==10497)return "repeat";if(value==33071)return "clamp";if(value==33648)return "mirror";
            fail("sampler wrap");return {};
        };
        target.wrapU=wrap(input.wrapS);target.wrapV=wrap(input.wrapT);
        const int minimum=input.minFilter<0?9987:input.minFilter,maximum=input.magFilter<0?9729:input.magFilter;
        const bool nearest=minimum==9728||minimum==9984||minimum==9986;
        if(minimum!=9728&&minimum!=9729&&minimum!=9984&&minimum!=9985&&minimum!=9986&&minimum!=9987)fail("sampler minFilter");
        if(maximum!=(nearest?9728:9729))fail("independent min/mag filter requires unsupported sampler state");
        target.filter=nearest?"nearest":"linear";
        target.mipPolicy=(minimum==9728||minimum==9729)?"none":"generate";
        target.mipFilter=(minimum==9986||minimum==9987)?azurerender::TextureImportSettings::MipFilter::Linear
            :azurerender::TextureImportSettings::MipFilter::Nearest;
    };
    const auto bind=[&](const azurerender::UnlitTextureLayer& layer,unsigned slot,int& imageIndex) {
        const auto& texture=model.textures[static_cast<std::size_t>(layer.texture)];
        if(!texture.extensions.empty()||texture.source<0||static_cast<std::size_t>(texture.source)>=model.images.size())fail("texture resource");
        const auto& image=model.images[static_cast<std::size_t>(texture.source)];
        if(image.width<=0||image.height<=0||image.image.empty())fail("texture image data");
        sampler(texture.sampler,result.textureSettings[slot]);imageIndex=texture.source;
    };
    for(unsigned i=0;i<layers.layerCount;++i)bind(layers.layers[i],i==0?0:4,i==0?baseImage:secondImage);
    if(layers.coverageLayer.present)bind(layers.coverageLayer.sample,8,coverageImage);
    result.unlitTextureLayers=std::move(layers);
}
