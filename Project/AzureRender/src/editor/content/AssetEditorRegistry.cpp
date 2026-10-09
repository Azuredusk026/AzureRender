#include "AssetEditorRegistry.hpp"
#include "runtime/AssetTypeRegistry.hpp"
namespace azurerender {
void AssetEditorRegistry::add(AssetEditorDescriptor descriptor){
    if(descriptor.type.empty()||descriptor.label.empty()||descriptor.openOperation.empty())throw std::invalid_argument("Asset editor requires type, label and operation");
    const auto id=descriptor.type;if(!entries_.emplace(id,std::move(descriptor)).second)throw std::invalid_argument("Duplicate asset editor");
}
nlohmann::json AssetEditorRegistry::open(const std::string& type,const std::string& asset) const{
    const auto found=entries_.find(type);if(found==entries_.end())throw std::invalid_argument("No editor registered for asset type: "+type);
    const auto& descriptor=found->second;return {{"asset",asset},{"type",type},{"label",descriptor.label},{"operation",descriptor.openOperation}};
}
AssetEditorRegistry AssetEditorRegistry::builtins(){
    AssetEditorRegistry result;for(const auto& [id,type]:assetTypeRegistry().types())result.add({id,type.label,id=="image"?"texture.describe":id=="model"?"preview.asset":"asset.inspect"});return result;
}
}
