#include "MaterialData.hpp"
#include "assets/MaterialGraph.hpp"
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
MaterialData MaterialData::defaults(const std::string& type){MaterialData result;result.typeId=type;for(const auto& field:MaterialTypeRegistry::builtins().type(type).parameters.items())result.parameters[field.key()]=field.value().at("default");return result;}
MaterialData MaterialData::parse(const Json& source){
    if(!source.is_object()||source.at("schemaVersion")!=kSchemaVersion)throw std::invalid_argument("Unsupported material schema");
    for(const auto& field:source.items())if(field.key()!="schemaVersion"&&field.key()!="typeId"&&field.key()!="parameters"&&field.key()!="textures"&&field.key()!="authoringGraph")throw std::invalid_argument("Unknown material field: "+field.key());
    auto result=defaults(source.at("typeId").get<std::string>());result.parameters=source.at("parameters");result.textures=source.value("textures",Json::object());result.authoringGraph=source.value("authoringGraph",Json(nullptr));result.validate();return result;
}
MaterialData MaterialData::load(const std::filesystem::path& path){std::ifstream input(path);if(!input)throw std::runtime_error("Cannot open material: "+path.u8string());Json data;input>>data;return parse(data);}
Json MaterialData::encode()const{validate();Json out={{"schemaVersion",kSchemaVersion},{"typeId",typeId},{"parameters",parameters},{"textures",textures}};if(!authoringGraph.is_null())out["authoringGraph"]=authoringGraph;return out;}
void MaterialData::validate()const{
    if(!authoringGraph.is_null()){const auto compiled=MaterialGraph::parse(authoringGraph).compile();if(compiled.typeId!=typeId||compiled.parameters!=parameters||compiled.textures!=textures)throw std::invalid_argument("Material graph and compiled values differ");}
    const auto& type=MaterialTypeRegistry::builtins().type(typeId);
    if(!parameters.is_object()||!textures.is_object()||parameters.size()!=type.parameters.size())throw std::invalid_argument("Material parameters require all registered fields");
    for(const auto& field:type.parameters.items()){
        const auto& value=parameters.at(field.key());const auto kind=field.value().at("type").get<std::string>();
        if(kind=="boolean"){if(!value.is_boolean())throw std::invalid_argument("Material boolean required");continue;}
        auto number=[&](const Json& item){if(!item.is_number()||!std::isfinite(item.get<double>())||item.get<double>()<field.value().at("min").get<double>()||item.get<double>()>field.value().at("max").get<double>())throw std::invalid_argument("Invalid material parameter: "+field.key());};
        if(kind=="color"){if(!value.is_array()||value.size()!=4)throw std::invalid_argument("Material color requires four channels");for(const auto& item:value)number(item);}else number(value);
    }
    for(const auto& field:textures.items()){
        if(!type.textures.contains(field.key())||!field.value().is_object()||field.value().size()!=2||field.value().at("usage")!=type.textures.at(field.key()))throw std::invalid_argument("Invalid material texture slot or usage");
        const auto id=field.value().at("asset").get<std::string>();
        if(id.size()!=36||id[8]!='-'||id[13]!='-'||id[18]!='-'||id[23]!='-')throw std::invalid_argument("Material textures require asset UUIDs");
        for(std::size_t index=0;index<id.size();++index)if(index!=8&&index!=13&&index!=18&&index!=23&&!std::isxdigit(static_cast<unsigned char>(id[index])))throw std::invalid_argument("Invalid material texture UUID");
    }
}
AssetMaterial MaterialData::interpret(const std::function<std::filesystem::path(const std::string&)>& resolve)const{
    validate();AssetMaterial result;result.name=typeId;result.materialProfileExplicit=true;
    auto image=[&](const std::string& slot,std::vector<std::uint8_t>& pixels,std::uint32_t& width,std::uint32_t& height,std::array<std::uint8_t,4> fallback){
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
    const auto color=parameters.at("baseColor").get<std::array<float,4>>();
    for(std::size_t i=0;i<result.baseColorPixels.size();++i){const auto channel=i%4;double value=result.baseColorPixels[i]/255.;const bool srgb=channel!=3&&result.textureSettings[0].colorSpace=="srgb";if(srgb)value=value<=.04045?value/12.92:std::pow((value+.055)/1.055,2.4);value*=color[channel];if(srgb)value=value<=.0031308?value*12.92:1.055*std::pow(value,1/2.4)-.055;result.baseColorPixels[i]=static_cast<std::uint8_t>(std::lround(std::clamp(value,0.,1.)*255));}
    image("normal",result.normalPixels,result.normalWidth,result.normalHeight,{128,128,255,255});
    image("metallicRoughness",result.metallicRoughnessPixels,result.metallicRoughnessWidth,result.metallicRoughnessHeight,{255,255,255,255});
    for(std::size_t i=0;i<result.metallicRoughnessPixels.size();i+=4){result.metallicRoughnessPixels[i+1]=static_cast<std::uint8_t>(std::lround(result.metallicRoughnessPixels[i+1]*parameters.at("roughness").get<double>()));result.metallicRoughnessPixels[i+2]=static_cast<std::uint8_t>(std::lround(result.metallicRoughnessPixels[i+2]*parameters.at("metallic").get<double>()));}
    image("emissive",result.specularEmissivePixels,result.specularEmissiveWidth,result.specularEmissiveHeight,{0,0,0,255});
    result.styleMaskWidth=result.styleMaskHeight=result.matcapWidth=result.matcapHeight=result.hairDataWidth=result.hairDataHeight=1;
    result.styleMaskPixels={255,255,255,255};result.matcapPixels={255,255,255,255};result.hairDataPixels={128,128,255,255};
    result.emissiveStrength=parameters.at("emissiveStrength").get<float>();result.doubleSided=parameters.at("doubleSided").get<bool>();
    result.transmission=parameters.value("transmission",0.F);result.ior=parameters.value("ior",1.5F);result.thickness=parameters.value("thickness",0.F);result.attenuationDistance=parameters.value("attenuationDistance",0.F);
    if(parameters.contains("attenuationColor"))for(unsigned c=0;c<3;++c)result.attenuationColor[c]=parameters.at("attenuationColor")[c].get<float>();
    MaterialTypeRegistry::builtins().type(typeId).interpret(result,parameters);return result;
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
        mesh.materials[index]=MaterialData::load(std::filesystem::u8path(binding.path)).interpret([&](const std::string& uuid){return std::filesystem::u8path(binding.textures.at(uuid));});mesh.materials[index].slotId=slotId;mesh.materials[index].name=name;
    }
}
}
