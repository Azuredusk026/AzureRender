#include "MaterialData.hpp"
#include "assets/MaterialGraph.hpp"
#include "assets/MaterialFrame.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <set>
namespace azurerender {
using Json=nlohmann::json;
void MaterialTypeRegistry::add(MaterialTypeDescriptor value){
    if(value.id.empty()||value.label.empty()||!value.interpret||!value.parameters.is_object()||!value.textures.is_object())throw std::invalid_argument("Incomplete material type");
    const auto id=value.id;if(!types_.emplace(id,std::move(value)).second)throw std::invalid_argument("Duplicate material type");
}
const MaterialTypeDescriptor& MaterialTypeRegistry::type(const std::string& id)const{
    const auto found=types_.find(id);if(found==types_.end())throw std::invalid_argument("Unknown material type: "+id);return found->second;
}
Json MaterialTypeRegistry::describe()const{auto out=Json::array();for(const auto& [id,value]:types_)out.push_back({{"id",id},{"label",value.label},{"parameters",value.parameters},{"textures",value.textures}});return out;}
MaterialTypeRegistry& MaterialTypeRegistry::builtins(){
    static auto registry=[] {
        MaterialTypeRegistry out;
        out.add({"azure.profile","Surface profile",Json::object(),Json::object(),[](AssetMaterial&,const Json&){}});
        Json parameters={{"baseColor",{{"type","color"},{"default",{.8,.8,.8,1.}},{"min",0},{"max",1}}},
            {"roughness",{{"type","number"},{"default",.75},{"min",0},{"max",1}}},
            {"metallic",{{"type","number"},{"default",0.},{"min",0},{"max",1}}},
            {"emissiveStrength",{{"type","number"},{"default",0.},{"min",0},{"max",32}}},
            {"transmission",{{"type","number"},{"default",0.},{"min",0},{"max",1}}},
            {"ior",{{"type","number"},{"default",1.5},{"min",1},{"max",3}}},
            {"thickness",{{"type","number"},{"default",0.},{"min",0},{"max",1000}}},
            {"attenuationDistance",{{"type","number"},{"default",0.},{"min",0},{"max",100000}}},
            {"attenuationColor",{{"type","color"},{"default",{1.,1.,1.,1.}},{"min",0},{"max",1}}},
            {"doubleSided",{{"type","boolean"},{"default",false}}}};
        Json textures={{"albedo","albedo"},{"normal","normal"},{"metallicRoughness","roughness"},{"emissive","emissive"}};
        out.add({"azure.pbr","PBR",parameters,textures,[](AssetMaterial& result,const Json&){result.materialFeatures=0;result.styleParameters={0,0,1,0};}});
        for(const auto* name:{"toonAmount","specularAmount","rimAmount"})parameters[name]={{"type","number"},{"default",1.},{"min",0},{"max",2}};
        out.add({"azure.stylized","Stylized",parameters,textures,[](AssetMaterial& result,const Json& values){
            result.materialFeatures=MaterialFeatureStylizedShadow;result.styleParameters={values.at("toonAmount").get<float>(),1,values.at("specularAmount").get<float>(),values.at("rimAmount").get<float>()};
        }});return out;
    }();return registry;
}
MaterialData MaterialData::defaults(const std::string& type){MaterialData result;result.typeId=type;for(const auto& field:MaterialTypeRegistry::builtins().type(type).parameters.items())result.parameters[field.key()]=field.value().at("default");if(type=="azure.profile")result.surface={{"asset",{{"version","2.0"}}},{"materials",Json::array({{{"extras",{{"azureRenderMaterial",{{"schemaVersion",2},{"class","generic"},{"features",Json::array()}}}}}}})}};return result;}
MaterialData MaterialData::parse(const Json& source){
    if(!source.is_object()||!source.at("schemaVersion").is_number_integer()
        ||(source.at("schemaVersion")!=1&&source.at("schemaVersion")!=2&&source.at("schemaVersion")!=3))throw std::invalid_argument("Unsupported material schema");
    const bool patch=source.at("schemaVersion")==2;
    if(patch?(!source.contains("mode")||source.at("mode")!="parameters"):source.contains("mode"))throw std::invalid_argument("Invalid material mode");
    for(const auto& field:source.items())if(field.key()!="schemaVersion"&&field.key()!="mode"&&field.key()!="typeId"&&field.key()!="parameters"&&field.key()!="textures"&&field.key()!="authoringGraph"&&field.key()!="surface")throw std::invalid_argument("Unknown material field: "+field.key());
    if((source.at("schemaVersion")==3)!=(source.at("typeId")=="azure.profile")||source.contains("surface")!=(source.at("schemaVersion")==3))throw std::invalid_argument("Surface profiles require material schema 3");
    auto result=defaults(source.at("typeId").get<std::string>());result.parameterOverride=patch;result.parameters=source.at("parameters");result.textures=source.value("textures",Json::object());result.authoringGraph=source.value("authoringGraph",Json(nullptr));result.surface=source.value("surface",Json(nullptr));result.validate();return result;
}
MaterialData MaterialData::load(const std::filesystem::path& path){std::ifstream input(path);if(!input)throw std::runtime_error("Cannot open material: "+path.u8string());Json data;input>>data;return parse(data);}
Json MaterialData::encode()const{validate();Json out={{"schemaVersion",typeId=="azure.profile"?3:parameterOverride?2:1},{"typeId",typeId},{"parameters",parameters},{"textures",textures}};if(parameterOverride)out["mode"]="parameters";if(!surface.is_null())out["surface"]=surface;if(!authoringGraph.is_null())out["authoringGraph"]=authoringGraph;return out;}
void MaterialData::validate()const{
    if(typeId=="azure.profile"){
        if(parameterOverride||!parameters.is_object()||!parameters.empty()||!surface.is_object()||!surface.contains("materials")||!surface["materials"].is_array()||surface["materials"].size()!=1)throw std::invalid_argument("Surface document requires exactly one material");
        for(const auto& field:surface.items())if(field.key()!="asset"&&field.key()!="materials"&&field.key()!="textures"&&field.key()!="samplers"&&field.key()!="nodes")throw std::invalid_argument("Unknown surface document field: "+field.key());
        if(!textures.is_object()||textures.size()>64)throw std::invalid_argument("Profile image table exceeds budget");
        for(std::size_t index=0;index<textures.size();++index)if(!textures.contains(std::to_string(index)))throw std::invalid_argument("Profile images require contiguous numeric keys");
    }else if(!surface.is_null())throw std::invalid_argument("Surface data requires a profile material");
    if(!authoringGraph.is_null()){const auto compiled=MaterialGraph::parse(authoringGraph).compile();if(compiled.surface!=surface||compiled.parameterOverride!=parameterOverride||compiled.typeId!=typeId||compiled.parameters!=parameters||compiled.textures!=textures)throw std::invalid_argument("Material graph and compiled values differ");}
    const auto& type=MaterialTypeRegistry::builtins().type(typeId);
    if(!parameters.is_object()||!textures.is_object()||(!parameterOverride&&parameters.size()!=type.parameters.size()))throw std::invalid_argument("Material parameters require all registered fields");
    for(const auto& field:parameters.items())if(!type.parameters.contains(field.key()))throw std::invalid_argument("Unknown material parameter: "+field.key());
    for(const auto& field:type.parameters.items()){
        if(parameterOverride&&!parameters.contains(field.key()))continue;
        const auto& value=parameters.at(field.key());const auto kind=field.value().at("type").get<std::string>();
        if(kind=="boolean"){if(!value.is_boolean())throw std::invalid_argument("Material boolean required");continue;}
        auto number=[&](const Json& item){if(!item.is_number()||!std::isfinite(item.get<double>())||item.get<double>()<field.value().at("min").get<double>()||item.get<double>()>field.value().at("max").get<double>())throw std::invalid_argument("Invalid material parameter: "+field.key());};
        if(kind=="color"){if(!value.is_array()||value.size()!=4)throw std::invalid_argument("Material color requires four channels");for(const auto& item:value)number(item);}else number(value);
    }
    for(const auto& field:textures.items()){
        if((typeId!="azure.profile"&&(!type.textures.contains(field.key())||field.value().at("usage")!=type.textures.at(field.key())))||!field.value().is_object()||field.value().size()!=2)throw std::invalid_argument("Invalid material texture slot or usage");
        if(typeId=="azure.profile"){
            const auto usage=field.value().at("usage").get<std::string>();
            if(usage!="albedo"&&usage!="normal"&&usage!="roughness"&&usage!="emissive"&&usage!="linear"&&usage!="mask"&&usage!="packed-normal")throw std::invalid_argument("Invalid profile image usage");
        }
        const auto id=field.value().at("asset").get<std::string>();
        if(id.size()!=36||id[8]!='-'||id[13]!='-'||id[18]!='-'||id[23]!='-')throw std::invalid_argument("Material textures require asset UUIDs");
        for(std::size_t index=0;index<id.size();++index)if(index!=8&&index!=13&&index!=18&&index!=23&&!std::isxdigit(static_cast<unsigned char>(id[index])))throw std::invalid_argument("Invalid material texture UUID");
    }
}
AssetMaterial MaterialData::interpret(const std::function<std::filesystem::path(const std::string&)>& resolve,const AssetMaterial* source)const{
    if(typeId=="azure.profile"){
        validate();auto document=surface;document["images"]=Json::array();std::uintmax_t sourceBytes=0;
        std::vector<Json> imageSettings(textures.size());
        for(std::size_t index=0;index<textures.size();++index){
            const auto path=resolve(textures.at(std::to_string(index)).at("asset").get<std::string>());
            const auto size=std::filesystem::file_size(path);sourceBytes+=size;
            if(size>64*1024*1024||sourceBytes>64*1024*1024)throw std::invalid_argument("Profile images exceed aggregate source budget");
            std::ifstream input(path,std::ios::binary);if(!input)throw std::invalid_argument("Cannot open profile image");
            const std::vector<unsigned char> bytes{std::istreambuf_iterator<char>(input),{}};
            if(bytes.size()>64*1024*1024)throw std::invalid_argument("Profile image exceeds source budget");
            const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string encoded;unsigned accumulator=0;int bits=-6;
            for(unsigned char byte:bytes){accumulator=(accumulator<<8)|byte;bits+=8;while(bits>=0){encoded+=alphabet[(accumulator>>bits)&63];bits-=6;}}
            if(bits>-6)encoded+=alphabet[((accumulator<<8)>>(bits+8))&63];while(encoded.size()%4)encoded+='=';
            document["images"].push_back({{"uri","data:application/octet-stream;base64,"+encoded}});
            std::ifstream sidecar(std::filesystem::u8path(path.u8string()+".azmeta"));
            if(sidecar){Json metadata;sidecar>>metadata;const auto settings=metadata.value("settings",Json::object());
                if(settings.contains("texture"))imageSettings[index]=settings.at("texture");}
        }
        return loadEmbeddedGltfMaterials(document.dump(),false,imageSettings).at(0);
    }
    validate();if(parameterOverride&&!source)throw std::invalid_argument("Parameter override requires a source material");
    AssetMaterial result=parameterOverride?*source:AssetMaterial{};if(!parameterOverride){result.name=typeId;result.materialProfileExplicit=true;}
    if(parameterOverride&&(result.materialClass!=AssetMaterialClass::Generic
        ||result.specularInput.present||result.nprDiffuse.present||result.eyeSurface.present
        ||result.projectedHairRamp.present||result.faceSdf.present||result.outlineSurface.present
        ||(result.materialFeatures&(MaterialFeatureUnlitSurface|MaterialFeatureSceneRefraction|MaterialFeatureMultiplyComposite)))
        &&(parameters.value("transmission",0.F)>0||parameters.value("thickness",0.F)>0))
        throw std::invalid_argument("Transmission overrides require a generic source material");
    auto image=[&](const std::string& slot,std::vector<std::uint8_t>& pixels,std::uint32_t& width,std::uint32_t& height,std::array<std::uint8_t,4> fallback){
        if(parameterOverride&&!textures.contains(slot))return;
        width=height=1;pixels.assign(fallback.begin(),fallback.end());
        if(textures.contains(slot)){const auto loaded=loadAssetImage(resolve(textures.at(slot).at("asset").get<std::string>()).u8string());pixels=loaded.pixels;width=loaded.width;height=loaded.height;
            const std::map<std::string,std::size_t> slots={{"albedo",0},{"normal",1},{"metallicRoughness",2},{"emissive",3}};
            result.authoredMipLevels[slots.at(slot)]=static_cast<unsigned>(loaded.mipLevels.size());
            for(std::size_t level=1;level<loaded.mipLevels.size();++level)pixels.insert(pixels.end(),loaded.mipLevels[level].rgba8.begin(),loaded.mipLevels[level].rgba8.end());
            if(slot=="normal"&&loaded.sourceCompression=="bc5")reconstructBc5NormalPixels(pixels);}
    };
    image("albedo",result.baseColorPixels,result.baseColorWidth,result.baseColorHeight,{255,255,255,255});
    const std::map<std::string,std::size_t> textureSlots={{"albedo",0},{"normal",1},{"metallicRoughness",2},{"emissive",3}};
    for(const auto& texture:textures.items()){
        const auto index=textureSlots.at(texture.key());const auto path=resolve(texture.value().at("asset").get<std::string>());
        std::ifstream input(std::filesystem::u8path(path.u8string()+".azmeta"));
        if(input){Json metadata;input>>metadata;const auto settings=metadata.value("settings",Json::object());if(settings.contains("texture"))result.textureSettings[index]=TextureImportSettings::parse(settings.at("texture"),result.textureSettings[index]);}
        result.textureSettings[index].usage=texture.value().at("usage").get<std::string>();
        if(index==1||index==2)result.textureSettings[index].colorSpace="linear";
    }
    if(parameters.contains("baseColor")){
    const auto color=parameters.at("baseColor").get<std::array<float,4>>();
    for(std::size_t i=0;i<result.baseColorPixels.size();++i){const auto channel=i%4;double value=result.baseColorPixels[i]/255.;const bool srgb=channel!=3&&result.textureSettings[0].colorSpace=="srgb";if(srgb)value=value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);value*=color[channel];if(srgb)value=value<=.0031308?value*12.92:1.055*std::pow(value,1/2.4)-.055;result.baseColorPixels[i]=static_cast<std::uint8_t>(std::lround(std::clamp(value,0.,1.)*255));}
    }
    image("normal",result.normalPixels,result.normalWidth,result.normalHeight,{128,128,255,255});
    image("metallicRoughness",result.metallicRoughnessPixels,result.metallicRoughnessWidth,result.metallicRoughnessHeight,{255,255,255,255});
    for(std::size_t i=0;i<result.metallicRoughnessPixels.size();i+=4){result.metallicRoughnessPixels[i+1]=static_cast<std::uint8_t>(std::lround(result.metallicRoughnessPixels[i+1]*parameters.value("roughness",1.)));result.metallicRoughnessPixels[i+2]=static_cast<std::uint8_t>(std::lround(result.metallicRoughnessPixels[i+2]*parameters.value("metallic",1.)));}
    image("emissive",result.specularEmissivePixels,result.specularEmissiveWidth,result.specularEmissiveHeight,{0,0,0,255});
    if(!parameterOverride){
    result.styleMaskWidth=result.styleMaskHeight=result.matcapWidth=result.matcapHeight=result.hairDataWidth=result.hairDataHeight=1;
    result.styleMaskPixels={255,255,255,255};result.matcapPixels={255,255,255,255};result.hairDataPixels={128,128,255,255};
    }
    result.emissiveStrength=parameters.value("emissiveStrength",result.emissiveStrength);result.doubleSided=parameters.value("doubleSided",result.doubleSided);
    result.transmission=parameters.value("transmission",result.transmission);result.ior=parameters.value("ior",result.ior);result.thickness=parameters.value("thickness",result.thickness);result.attenuationDistance=parameters.value("attenuationDistance",result.attenuationDistance);
    if(parameters.contains("attenuationColor"))for(unsigned c=0;c<3;++c)result.attenuationColor[c]=parameters.at("attenuationColor")[c].get<float>();
    if(!parameterOverride)MaterialTypeRegistry::builtins().type(typeId).interpret(result,parameters);
    else {
        if(parameters.contains("toonAmount"))result.styleParameters[0]=parameters.at("toonAmount").get<float>();
        if(parameters.contains("specularAmount"))result.styleParameters[2]=parameters.at("specularAmount").get<float>();
        if(parameters.contains("rimAmount"))result.styleParameters[3]=parameters.at("rimAmount").get<float>();
        if(parameters.contains("emissiveStrength"))result.materialFeatures&=~MaterialFeatureEmissiveMask;
    }
    if((!parameterOverride||parameters.contains("emissiveStrength"))&&result.emissiveStrength>0)result.materialFeatures|=MaterialFeatureEmissiveMask;
    return result;
}
std::vector<std::string> inspectMaterialSlots(const std::filesystem::path& path){
    auto extension=path.extension().u8string();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char value){return static_cast<char>(std::tolower(value));});
    if(extension!=".glb"&&extension!=".gltf")return materialSlotIds(loadGltfAsset(path.u8string()));
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot inspect source material slots: "+path.u8string());
    Json document;
    if(extension==".glb"){
        unsigned char header[20]{};input.read(reinterpret_cast<char*>(header),sizeof(header));
        const auto word=[&](unsigned offset){std::uint32_t value=0;for(unsigned byte=0;byte<4;++byte)value|=std::uint32_t(header[offset+byte])<<(byte*8U);return value;};
        const auto bytes=std::filesystem::file_size(path);
        if(!input||word(0)!=0x46546c67U||word(4)!=2||word(8)<20||word(8)>bytes||word(16)!=0x4e4f534aU||word(12)>word(8)-20)
            throw std::invalid_argument("Invalid GLB material metadata header");
        std::string json(word(12),'\0');input.read(json.data(),static_cast<std::streamsize>(json.size()));
        if(!input)throw std::invalid_argument("Truncated GLB material metadata");document=Json::parse(json);
    }else input>>document;
    if(!document.is_object())throw std::invalid_argument("Material metadata requires a glTF object");
    const auto materials=document.value("materials",Json::array());
    if(!materials.is_array())throw std::invalid_argument("Material metadata requires an array");
    LoadedAsset metadata;metadata.materials.reserve(materials.size()+1);
    for(const auto& value:materials){
        if(!value.is_object())throw std::invalid_argument("Material metadata entries require objects");
        AssetMaterial material;material.name=value.value("name",std::string{});if(material.name.empty())material.name="UnnamedMaterial";
        const auto extras=value.value("extras",Json::object());
        if(extras.is_object()&&extras.contains("azureSlotId")){
            const auto& identity=extras.at("azureSlotId");
            if(!identity.is_string()||identity.get<std::string>().empty())throw std::invalid_argument("azureSlotId requires a nonempty string");
            material.slotId=identity.get<std::string>();
        }
        metadata.materials.push_back(std::move(material));
    }
    metadata.materials.emplace_back();
    return materialSlotIds(metadata);
}
std::vector<std::string> materialSlotIds(const LoadedAsset& mesh){
    std::map<std::string,unsigned> names;for(const auto& material:mesh.materials)++names[material.name];
    std::vector<std::string> result;std::set<std::string> unique;
    for(std::size_t i=0;i<mesh.materials.size();++i){const auto& material=mesh.materials[i];const auto id=!material.slotId.empty()?material.slotId:(!material.name.empty()&&names.at(material.name)==1?"name:"+material.name:"index:"+std::to_string(i));if(!unique.insert(id).second)throw std::invalid_argument("Duplicate source material slot identity");result.push_back(id);}return result;
}
void applyMaterialBindings(LoadedAsset& mesh,const std::map<std::string,MaterialBinding>& bindings){
    const auto slots=materialSlotIds(mesh);
    for(const auto& [id,binding]:bindings){const auto found=std::find(slots.begin(),slots.end(),id);if(found==slots.end())throw std::invalid_argument("Material slot disappeared during reimport: "+id);
        const auto index=static_cast<std::size_t>(found-slots.begin());const auto slotId=mesh.materials[index].slotId,name=mesh.materials[index].name;
        auto document=MaterialData::load(std::filesystem::u8path(binding.path));
        if(document.typeId=="azure.profile"){
            document.authoringGraph=nullptr;document.surface["nodes"]=Json::array();
            for(const auto& node:mesh.nodes)document.surface["nodes"].push_back({{"name",node.name}});
        }
        auto replacement=document.interpret([&](const std::string& uuid){return std::filesystem::u8path(binding.textures.at(uuid));},&mesh.materials[index]);
        const auto& source=mesh.materials[index];std::string losses;
        const auto lost=[&](bool condition,const char* field){if(condition){if(!losses.empty())losses+=", ";losses+=field;}};
        lost(source.materialClass!=replacement.materialClass,"class");
        lost(source.faceSdf.present&&!replacement.faceSdf.present,"faceSdf");
        lost(source.nprDiffuse.present&&!replacement.nprDiffuse.present,"nprDiffuse");
        lost(source.projectedHairRamp.present&&!replacement.projectedHairRamp.present,"hair.projectedRamp");
        lost(source.eyeSurface.present&&!replacement.eyeSurface.present,"eyeSurface");
        lost(source.outlineSurface.present&&!replacement.outlineSurface.present,"outlineSurface");
        lost(source.specularInput.present&&!replacement.specularInput.present,"specularInput");
        lost(source.specularRamp.present&&!replacement.specularRamp.present,"specularRamp");
        lost(source.alphaMode!=replacement.alphaMode,"alphaMode");
        lost(source.doubleSided!=replacement.doubleSided,"doubleSided");
        const auto removed=source.materialFeatures&~replacement.materialFeatures;
        for(const auto& [feature,label]:std::initializer_list<std::pair<std::uint32_t,const char*>>{
            {MaterialFeatureHairAnisotropy,"hair-anisotropy"},{MaterialFeatureHairRamp,"hair-ramp"},
            {MaterialFeatureDiffuseRamp,"diffuse-ramp"},{MaterialFeatureShadowColorLut,"shadow-color-lut"},
            {MaterialFeatureSilhouetteOutline,"silhouette-outline"},{MaterialFeatureMultiplyComposite,"multiply-composite"},
            {MaterialFeatureOverlay,"overlay"},{MaterialFeatureBrowOverlay,"brow-overlay"},
            {MaterialFeatureSceneRefraction,"scene-refraction"},{MaterialFeatureUnlitSurface,"unlit-surface"},
            {MaterialFeatureGeometryShadow,"geometry-shadow"},{MaterialFeatureSqrtAlphaShadow,"sqrt-alpha-shadow"},
            {MaterialFeatureHairPixelIntegral,"hair-pixel-integral"}})
            lost((removed&feature)!=0,label);
        replacement.slotId=slotId;replacement.name=name;mesh.materials[index]=std::move(replacement);
        if(!document.parameterOverride&&!losses.empty())RuntimeDiagnostics::instance().warning("material",
            "Full material replacement at slot "+id+" from "+binding.path+" discards source semantics: "+losses);
    }
    static_cast<void>(resolveMaterialFrame(mesh));
    validateOutlineMaterialInputs(mesh);
}
}
