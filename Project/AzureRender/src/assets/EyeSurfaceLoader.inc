// Included inside the glTF loader's implementation namespace.
// Scan source tokens without retaining a second document or material tree.
// TinyGLTF normalizes invalid optional alphaMode values to its OPAQUE default.
std::vector<bool> readAuthoredMaterialAlpha(const unsigned char* bytes,std::size_t size,bool binary) {
    if(!bytes||size==0)throw std::invalid_argument("Empty authored material JSON source");
    if(binary) {
        const auto word=[&](std::size_t offset) {
            std::uint32_t result=0;
            for(unsigned byte=0;byte<4;++byte)result|=std::uint32_t(bytes[offset+byte])<<(byte*8U);
            return result;
        };
        if(size<20||word(0)!=0x46546c67U||word(4)!=2||word(16)!=0x4e4f534aU||word(12)>size-20)
            throw std::invalid_argument("Invalid GLB authored material JSON chunk");
        size=word(12);bytes+=20;
    }
    using Json=nlohmann::json;using Event=Json::parse_event_t;
    std::vector<bool> opaque;
    bool materials=false,alpha=false;
    const auto callback=[&](int depth,Event event,Json& value) {
        if(depth==1&&event==Event::key) {
            materials=value=="materials";alpha=false;
            if(materials)opaque.clear();
        }
        if(materials&&depth==2&&(event==Event::object_start||event==Event::array_start||event==Event::value)) {
            opaque.push_back(true);alpha=false;
        }
        if(materials&&depth==3&&event==Event::key)alpha=value=="alphaMode";
        if(materials&&depth==3&&alpha&&!opaque.empty()
            &&(event==Event::value||event==Event::array_start||event==Event::object_start))
            opaque.back()=event==Event::value&&value.is_string()&&value=="OPAQUE";
        // Empty container frames keep scalar callbacks active. Discarded keys
        // and values retain only the parser stack and one bit per material.
        return event==Event::object_start||event==Event::array_start;
    };
    (void)Json::parse(bytes,bytes+size,callback);
    return opaque;
}

void validateEyeAuthoredAlpha(const tinygltf::Model& model,const std::vector<bool>& opaque) {
    for(std::size_t index=0;index<model.materials.size();++index) {
        const auto& material=model.materials[index];
        if(!material.extras.IsObject()||!material.extras.Has("azureRenderMaterial"))continue;
        const auto& profile=material.extras.Get("azureRenderMaterial");
        if(!profile.IsObject()||!profile.Has("features")||!profile.Get("features").IsArray())continue;
        const auto& features=profile.Get("features");
        for(std::size_t feature=0;feature<features.ArrayLen();++feature) {
            const auto& value=features.Get(static_cast<int>(feature));
            if(value.IsString()&&value.Get<std::string>()=="eye-radial-surface"
                &&(index>=opaque.size()||!opaque[index]))
                throw std::invalid_argument(material.name+" eyeSurface.alphaMode: require OPAQUE or an omitted field");
        }
    }
}

// Validate authored JSON before TinyGLTF can omit null array/object values.
void loadEyeSurface(const tinygltf::Material& material,const tinygltf::Model& model,
    AssetMaterial& result,int& rampImage,int& matcapImage) {
    const auto authored=material.extras_json_string.empty()?nlohmann::json::object()
        :nlohmann::json::parse(material.extras_json_string);
    const bool enabled=(result.materialFeatures&MaterialFeatureEyeRadialSurface)!=0;
    const bool declared=authored.is_object()&&authored.contains("azureRenderMaterial")
        &&authored["azureRenderMaterial"].is_object()
        &&authored["azureRenderMaterial"].contains("eyeSurface");
    if(!enabled&&!declared)return;
    const auto fail=[&](const std::string& property,const std::string& expectation) {
        throw std::invalid_argument(result.name+" eyeSurface."+property+": "+expectation);
    };
    if(!enabled||!declared)fail("feature","require eye-radial-surface and eyeSurface");
    const auto& profile=authored.at("azureRenderMaterial");
    const auto number=[&](const nlohmann::json& value,const std::string& property,double low,double high) {
        if(!value.is_number())fail(property,"require a finite number");
        const double result=value.get<double>();
        if(!std::isfinite(result)||result<low||result>high)fail(property,"number outside supported range");
        return static_cast<float>(result);
    };
    if(!profile.contains("schemaVersion")||number(profile["schemaVersion"],"schemaVersion",2,2)!=2
        ||!profile.contains("class")||profile["class"]!="eye"
        ||result.materialClass!=AssetMaterialClass::Eye||material.alphaMode!="OPAQUE")
        fail("material","require schema 2, eye class and OPAQUE");
    if(!profile.contains("features")||!profile["features"].is_array())fail("features","require an explicit string array");
    std::set<std::string> features;
    for(const auto& value:profile["features"]) {
        if(!value.is_string())fail("features","require nonempty strings");
        const auto name=value.get<std::string>();
        if(name!="eye-radial-surface"&&name!="silhouette-outline")fail("features","incompatible feature "+name);
        if(!features.insert(name).second)fail("features","duplicate feature "+name);
    }
    if(!features.count("eye-radial-surface"))fail("features","missing eye-radial-surface");
    constexpr std::uint32_t allowed=MaterialFeatureEyeRadialSurface|MaterialFeatureSilhouetteOutline;
    if((result.materialFeatures&~allowed)||result.transmission!=0||result.thickness!=0)
        fail("material","incompatible lighting, transmission or thickness");
    if(!material.extensions.empty())fail("extensions","material extension requires a separate eye adapter");
    for(const char* key:{"lamShadowColor","matcapColor","hair","faceSdf","diffuseRamp","refraction",
        "sceneRefraction","multiplyComposite","composite","shadowColorLut","surface"})
        if(profile.contains(key))fail(key,"texture or parameter owner conflict");
    for(const auto& property:authored.items())
        if(property.key().rfind("afterglow",0)==0)fail(property.key(),"legacy parameter owner conflict");
    if(material.normalTexture.index>=0||material.occlusionTexture.index>=0
        ||material.pbrMetallicRoughness.metallicRoughnessTexture.index>=0||material.emissiveTexture.index>=0)
        fail("textures","require base color, eye ramp and eye Matcap owners only");
    const auto& source=profile["eyeSurface"];
    const std::set<std::string> fields{"schemaVersion","diffuseRampTexture","matcapTexture","parallaxScale",
        "matcapNormalScale","alphaPremultiply","highlightColor","scatteringColor","matcapColor",
        "uvScaleOffset","lightingInputs"};
    if(!source.is_object())fail("object","require an object");
    for(const auto& field:source.items())if(!fields.count(field.key()))fail(field.key(),"unknown field");
    for(const auto& field:fields)if(!source.contains(field))fail(field,"required field missing");
    if(number(source["schemaVersion"],"schemaVersion",1,1)!=1)fail("schemaVersion","require version 1");
    auto& eye=result.eyeSurface;
    eye.parallaxScale=number(source["parallaxScale"],"parallaxScale",0,1);
    eye.matcapNormalScale=number(source["matcapNormalScale"],"matcapNormalScale",0,8);
    eye.alphaPremultiply=number(source["alphaPremultiply"],"alphaPremultiply",0,1);
    const auto vector=[&](const nlohmann::json& owner,const char* property,auto& output,double low,double high,
        const std::string& prefix=std::string{}) {
        if(!owner.contains(property)||!owner[property].is_array()||owner[property].size()!=output.size())
            fail(prefix+property,"incorrect vector length");
        for(std::size_t channel=0;channel<output.size();++channel)
            output[channel]=number(owner[property][channel],prefix+property,low,high);
    };
    vector(source,"highlightColor",eye.highlightColor,0,64);
    vector(source,"scatteringColor",eye.scatteringColor,0,64);
    vector(source,"matcapColor",eye.matcapColor,0,64);
    vector(source,"uvScaleOffset",eye.uvScaleOffset,-65536,65536);
    const auto& lighting=source["lightingInputs"];
    const std::set<std::string> lightFields{"source","illuminationModel","matcapShadowFloor","directWeights"};
    if(!lighting.is_object())fail("lightingInputs","require an object");
    for(const auto& field:lighting.items())if(!lightFields.count(field.key()))fail("lightingInputs."+field.key(),"unknown field");
    for(const auto& field:lightFields)if(!lighting.contains(field))fail("lightingInputs."+field,"required field missing");
    if(lighting["source"]!="host-authored"||lighting["illuminationModel"]!="host-lambert")
        fail("lightingInputs","require explicit host-authored host-lambert inputs");
    eye.matcapShadowFloor=number(lighting["matcapShadowFloor"],"lightingInputs.matcapShadowFloor",0,1);
    vector(lighting,"directWeights",eye.directWeights,0,64,"lightingInputs.");
    const auto sampler=[&](int index,const std::string& property,bool fixed) {
        azurerender::TextureImportSettings settings;
        settings.anisotropy=1;
        const auto& texture=model.textures.at(static_cast<std::size_t>(index));
        if(texture.sampler>=0) {
            if(static_cast<std::size_t>(texture.sampler)>=model.samplers.size())fail(property,"missing sampler");
            const auto& s=model.samplers[static_cast<std::size_t>(texture.sampler)];
            const auto wrap=[&](int mode) -> std::string {
                if(mode==33071)return "clamp";
                if(mode==33648)return "mirror";
                if(mode==10497)return "repeat";
                fail(property,"unsupported sampler address mode");return {};
            };
            settings.wrapU=wrap(s.wrapS);settings.wrapV=wrap(s.wrapT);
            if(s.magFilter!=9729&&s.magFilter!=-1)fail(property,"require linear magnification");
            if(s.minFilter!=9729&&s.minFilter!=9987&&s.minFilter!=-1)fail(property,"require linear minification");
            settings.mipPolicy=s.minFilter==9729?"none":"generate";
        }
        if(fixed&&(settings.wrapU!="clamp"||settings.wrapV!="clamp"||settings.mipPolicy!="none"))
            fail(property,"require explicit clamp, linear and no-mip sampler");
        return settings;
    };
    const auto texture=[&](const char* property,unsigned slot) {
        const auto& value=source[property];
        if(!value.is_number())fail(property,"require an integer texture index");
        const auto index=value.get<double>();
        if(!std::isfinite(index)||std::floor(index)!=index||index<0||index>=model.textures.size())
            fail(property,"texture index outside model");
        const auto& reference=model.textures[static_cast<std::size_t>(index)];
        const int image=reference.source;
        if(image<0||static_cast<std::size_t>(image)>=model.images.size())fail(property,"texture image missing");
        if(!reference.extensions.empty())fail(property,"texture extension requires a separate adapter");
        result.textureSettings[slot]=sampler(static_cast<int>(index),property,true);
        result.textureSettings[slot].colorSpace=slot==4?"linear":"srgb";
        result.textureSettings[slot].usage=slot==4?"linear":"albedo";
        return image;
    };
    rampImage=texture("diffuseRampTexture",4);matcapImage=texture("matcapTexture",5);
    const auto& ramp=model.images.at(static_cast<std::size_t>(rampImage));
    if(ramp.width<2||ramp.height!=1)fail("diffuseRampTexture","require a horizontal row at least two texels wide");
    const auto& matcap=model.images.at(static_cast<std::size_t>(matcapImage));
    if(matcap.width<1||matcap.height<1)fail("matcapTexture","require a decoded image");
    const auto& base=material.pbrMetallicRoughness.baseColorTexture;
    if(base.texCoord!=0||!base.extensions.empty())fail("baseColorTexture","require UV0 and eyeSurface UV transform");
    if(base.index>=0) {
        const auto& reference=model.textures.at(static_cast<std::size_t>(base.index));
        if(reference.source<0||static_cast<std::size_t>(reference.source)>=model.images.size())
            fail("baseColorTexture","texture image missing");
        if(!reference.extensions.empty())fail("baseColorTexture","texture extension requires a separate adapter");
        result.textureSettings[0]=sampler(base.index,"baseColorTexture",false);
        result.textureSettings[0].colorSpace="srgb";result.textureSettings[0].usage="albedo";
    }
    eye.present=true;
}

void validateEyeTextureSettings(const AssetMaterial& material) {
    if(!material.eyeSurface.present)return;
    if(material.textureSettings[0].colorSpace!="srgb"||material.textureSettings[4].colorSpace!="linear"
        ||material.textureSettings[5].colorSpace!="srgb")
        throw std::invalid_argument(material.name+" eyeSurface.textures: fixed color-space ownership violated");
    for(unsigned slot:{4U,5U}) {
        const auto& settings=material.textureSettings[slot];
        if(settings.wrapU!="clamp"||settings.wrapV!="clamp"||settings.filter!="linear"
            ||settings.mipPolicy!="none"||settings.anisotropy!=1)
            throw std::invalid_argument(material.name+" eyeSurface.textures: fixed eye sampler violated");
    }
}
