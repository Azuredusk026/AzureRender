#include "ThumbnailProviderRegistry.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include "runtime/MaterialDocument.hpp"
#include <fstream>
namespace azurerender {
namespace {
std::filesystem::path primitive(const std::filesystem::path& cache,const std::string& kind){
    std::filesystem::create_directories(cache);const auto path=cache/(kind+"-v1.glb");
    if(!std::filesystem::exists(path)){GenerationRequest request;request.parameters={{"kind",kind},{"size",{2,2,2}},{"segments",32}};request.outputReference="preview:/"+kind+"-v1.glb";request.licenseSource="Azure primitive generator v1";
        const auto result=GeneratorRegistry::builtins().generate("azure.primitive",request);std::ofstream out(path,std::ios::binary);out.write(result.bytes.data(),static_cast<std::streamsize>(result.bytes.size()));out.flush();if(!out)throw std::runtime_error("Cannot write preview geometry");}
    return path;
}
ThumbnailDescription base(const AssetRecord& record,const RenderViewDescriptor& settings){
    ThumbnailDescription result;result.view=settings;result.view.rendererId="character";result.view.extent={128,128};result.view.scene={};result.view.sceneWorldCoordinates=true;
    result.view.settings.characterPresentation.platformEnabled=false;result.view.settings.characterPresentation.backgroundEnabled=true;
    result.view.cameraPosition={2.8F,2.1F,3.2F};result.view.cameraTarget={0,0,0};
    result.key={record.id,std::to_string(record.fingerprint)+":material-interpreter-v1","128x128-provider-v1"};return result;
}
void place(ThumbnailDescription& result,const std::string& id,const std::filesystem::path& path,std::map<std::string,MaterialBinding> materials={}){
    result.view.scene.resources.push_back({id,path.u8string(),std::move(materials)});scene::SceneNodeDesc node;node.id="preview";node.resourceId=id;result.view.scene.nodes.push_back(node);
}
}
void ThumbnailProviderRegistry::add(std::string type,Provider provider){if(type.empty()||!provider||!providers_.emplace(type,std::move(provider)).second)throw std::invalid_argument("Invalid or duplicate thumbnail provider");}
ThumbnailDescription ThumbnailProviderRegistry::describe(const AssetRecord& record,const AssetDatabase& assets,const std::filesystem::path& cache,const RenderViewDescriptor& settings)const{
    const auto* type=assetTypeRegistry().classify(record.path);if(!type||!providers_.count(type->id))throw std::invalid_argument("No preview provider for this asset type");return providers_.at(type->id)(record,assets,cache,settings);
}
nlohmann::json ThumbnailProviderRegistry::types()const{auto result=nlohmann::json::array();for(const auto& [type,provider]:providers_){static_cast<void>(provider);result.push_back(type);}return result;}
ThumbnailProviderRegistry& ThumbnailProviderRegistry::builtins(){
    static auto value=[] {
        ThumbnailProviderRegistry result;
        result.add("model",[](const auto& record,const auto&,const auto&,const auto& settings){auto out=base(record,settings);out.view.sceneWorldCoordinates=false;place(out,record.id,record.path);return out;});
        result.add("material",[](const auto& record,const auto& assets,const auto& cache,const auto& settings){auto out=base(record,settings);const auto path=primitive(cache,"sphere");const auto slots=materialSlotIds(loadGltfAsset(path.u8string()));const auto binding=resolveMaterialBinding(record.id,assets);place(out,record.id,path,{{slots.at(0),binding}});out.key.fingerprint=binding.fingerprint+":material-interpreter-v1";return out;});
        result.add("image",[](const auto& record,const auto&,const auto& cache,const auto& settings){auto out=base(record,settings);const auto geometry=primitive(cache,"plane");const auto image=loadAssetImage(record.path.u8string());
            auto document=MaterialDocument::defaults("azure.pbr");document.parameters["baseColor"]={1,1,1,1};document.parameters["doubleSided"]=true;document.textures["albedo"]={{"asset",record.id},{"usage","albedo"}};
            const auto path=cache/(record.id+"-"+std::to_string(record.fingerprint)+".azurematerial");if(!std::filesystem::exists(path)){std::ofstream file(path);file<<document.encode().dump(2);file.flush();if(!file)throw std::runtime_error("Cannot write image preview descriptor");}
            MaterialBinding binding;binding.path=path.u8string();binding.fingerprint=std::to_string(record.fingerprint);binding.textures[record.id]=record.path.u8string();
            place(out,record.id,geometry,{{materialSlotIds(loadGltfAsset(geometry.u8string())).at(0),binding}});
            const float aspect=static_cast<float>(image.width)/image.height;out.view.scene.nodes[0].scale={std::min(aspect,1.F),1,std::min(1.F/aspect,1.F)};out.view.cameraPosition={0,3,0.001F};return out;});
        return result;
    }();return value;
}
}
