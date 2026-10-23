// Included in the glTF loader implementation after the image decoder.
void loadOutlineSurface(const tinygltf::Material& material,const tinygltf::Model& model,
    AssetMaterial& result,
    const std::vector<nlohmann::json>* imageSettings) {
    if(material.extras_json_string.empty())return;
    const auto extras=nlohmann::json::parse(material.extras_json_string);
    if(!extras.contains("azureRenderMaterial"))return;
    const auto& profile=extras["azureRenderMaterial"];
    if(!profile.contains("outlineSurface"))return;
    const auto fail=[&](const std::string& field) {
        throw std::invalid_argument(result.name+" outlineSurface."+field+": invalid outline input");
    };
    if(result.alphaMode==AssetAlphaMode::Blend||result.materialClass==AssetMaterialClass::Overlay)
        fail("material");
    const auto& source=profile["outlineSurface"];
    const std::set<std::string> fields{"schemaVersion","mode","width","minimumPixels","depthOffset",
        "normalSource","depthPrepass","maskTexture","uvScaleOffset","color"};
    if(!source.is_object())fail("object");
    for(const auto& item:source.items())if(!fields.count(item.key()))fail(item.key());
    if(!source.contains("schemaVersion")||!source["schemaVersion"].is_number_integer()
        ||source["schemaVersion"]!=1)fail("schemaVersion");
    if(!source.contains("mode")||source["mode"]!="view-scaled")fail("mode");
    const auto number=[&](const nlohmann::json& value,const char* field,double low,double high) {
        if(!value.is_number())fail(field);
        const double result=value.get<double>();
        if(!std::isfinite(result)||result<low||result>high)fail(field);
        return static_cast<float>(result);
    };
    auto& target=result.outlineSurface;
    if(!source.contains("width"))fail("width");
    target.width=number(source["width"],"width",0,8);
    if(source.contains("minimumPixels"))target.minimumPixels=number(source["minimumPixels"],"minimumPixels",0,8);
    if(source.contains("depthOffset"))target.depthOffset=number(source["depthOffset"],"depthOffset",-4,4);
    if(source.contains("normalSource")) {
        if(source["normalSource"]=="tangent-uv1")target.normalSource=AssetMaterial::OutlineSurface::NormalSource::TangentUv1;
        else if(source["normalSource"]=="tangent-octahedral-uv1")target.normalSource=AssetMaterial::OutlineSurface::NormalSource::TangentOctahedralUv1;
        else if(source["normalSource"]!="geometry")fail("normalSource");
    }
    if(source.contains("depthPrepass")) {
        if(!source["depthPrepass"].is_boolean())fail("depthPrepass");
        target.depthPrepass=source["depthPrepass"].get<bool>();
    }
    if(!(result.materialFeatures&MaterialFeatureSilhouetteOutline)&&!target.depthPrepass)fail("participation");
    const auto vector=[&](const char* field,auto& output,double low,double high) {
        if(!source.contains(field))return;
        const auto& values=source[field];
        if(!values.is_array()||values.size()!=output.size())fail(field);
        for(std::size_t i=0;i<output.size();++i)output[i]=number(values[i],field,low,high);
    };
    vector("uvScaleOffset",target.uvScaleOffset,-65536,65536);
    vector("color",target.color,0,64);
    if(source.contains("maskTexture")) {
        const auto& raw=source["maskTexture"];
        if(!raw.is_number())fail("maskTexture");
        const double value=raw.get<double>();
        if(!std::isfinite(value)||value<0||std::floor(value)!=value||value>=model.textures.size())fail("maskTexture");
        const auto& texture=model.textures.at(static_cast<std::size_t>(value));
        if(!texture.extensions.empty()||texture.source<0||static_cast<std::size_t>(texture.source)>=model.images.size())fail("maskTexture");
        if(texture.sampler>=0) {
            if(static_cast<std::size_t>(texture.sampler)>=model.samplers.size())fail("maskTexture.sampler");
            const auto& sampler=model.samplers[static_cast<std::size_t>(texture.sampler)];
            if(sampler.wrapS!=33071||sampler.wrapT!=33071
                ||(sampler.magFilter!=-1&&sampler.magFilter!=9729)
                ||(sampler.minFilter!=-1&&sampler.minFilter!=9729))fail("maskTexture.sampler");
        }
        if(imageSettings&&!imageSettings->at(static_cast<std::size_t>(texture.source)).is_null()) {
            azurerender::TextureImportSettings defaults;defaults.usage="linear";defaults.colorSpace="linear";
            defaults.wrapU=defaults.wrapV="clamp";defaults.mipPolicy="none";defaults.anisotropy=1;
            azurerender::TextureImportSettings settings;
            try {settings=azurerender::TextureImportSettings::parse(imageSettings->at(static_cast<std::size_t>(texture.source)),defaults);}
            catch(const std::exception& error) {
                throw std::invalid_argument(result.name+" outlineSurface.maskTexture.settings: "+error.what());
            }
            if(settings.colorSpace!="linear"||settings.filter!="linear"||settings.wrapU!="clamp"||settings.wrapV!="clamp"
                ||settings.mipPolicy!="none"||settings.anisotropy!=1||settings.mipBias!=0)fail("maskTexture.settings");
        }
        target.maskPixels=decodeImageRgba(model,texture.source,{1,1,1,1},target.maskWidth,target.maskHeight,false);
    }
    target.present=true;
}
