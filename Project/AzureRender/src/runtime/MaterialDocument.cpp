#include "MaterialDocument.hpp"
#include "runtime/AssetTypeRegistry.hpp"
namespace azurerender {
MaterialBinding resolveMaterialBinding(const std::string& id,const AssetDatabase& assets){
    const auto& record=assets.records().at(id);
    if(!assetTypeRegistry().matches(record.path,{"material"}))throw std::invalid_argument("Slot overrides require material asset UUIDs");
    const auto document=MaterialDocument::load(record.path);MaterialBinding binding;
    binding.path=record.path.u8string();binding.fingerprint=std::to_string(record.fingerprint);
    for(const auto& texture:document.textures.items()){
        const auto uuid=texture.value().at("asset").get<std::string>();const auto path=assets.resolve(uuid);
        if(!assetTypeRegistry().matches(path,{"image"}))throw std::invalid_argument("Material texture UUID must reference an image");
        binding.textures[uuid]=path.u8string();binding.fingerprint+=":"+std::to_string(assets.records().at(uuid).fingerprint);
    }return binding;
}
void resolveSceneMaterials(SceneDocument& scene,const AssetDatabase& assets){
    auto candidate=scene;
    std::map<std::string,std::vector<std::string>> modelSlots;
    for(auto& node:candidate.nodes){
        node.resolvedMaterials.clear();if(node.materialSlots.empty())continue;
        const auto resource=std::find_if(candidate.resources.begin(),candidate.resources.end(),[&](const auto& item){return item.id==node.resourceId;});
        if(resource==candidate.resources.end())throw std::invalid_argument("Material overrides require a mesh");
        const auto key=resource->path.u8string();if(!modelSlots.count(key))modelSlots.emplace(key,inspectMaterialSlots(resource->path));
        const auto& slots=modelSlots.at(key);
        for(const auto& [slot,id]:node.materialSlots){
            if(std::find(slots.begin(),slots.end(),slot)==slots.end())throw std::invalid_argument("Material slot disappeared during reimport: "+slot);
            node.resolvedMaterials[slot]=resolveMaterialBinding(id,assets);
        }
    }
    scene=std::move(candidate);
}
}
