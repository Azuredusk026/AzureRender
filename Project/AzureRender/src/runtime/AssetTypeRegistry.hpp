#pragma once
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace azurerender {
struct AssetTypeDescriptor { std::string id,label;std::vector<std::string> extensions;bool placeable=false; };
class AssetTypeRegistry {
public:
    void add(AssetTypeDescriptor descriptor) {
        if(descriptor.id.empty()||descriptor.extensions.empty())throw std::invalid_argument("Asset types require an identity and extensions");
        for(auto& extension:descriptor.extensions){if(extension.empty()||extension[0]!='.')throw std::invalid_argument("Asset extensions require a leading dot");std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});}
        const auto id=descriptor.id;if(!types_.emplace(id,std::move(descriptor)).second)throw std::invalid_argument("Duplicate asset type: "+id);
    }
    bool matches(const std::filesystem::path& path,const std::vector<std::string>& types) const {
        if(types.empty())return true;
        const auto& actual=classify(path);
        return actual&&std::find(types.begin(),types.end(),actual->id)!=types.end();
    }
    const AssetTypeDescriptor* classify(const std::filesystem::path& path) const {
        auto extension=path.extension().u8string();std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
        for(const auto& [id,type]:types_)if(std::find(type.extensions.begin(),type.extensions.end(),extension)!=type.extensions.end())return &type;
        return nullptr;
    }
    const auto& types() const{return types_;}
private:
    std::map<std::string,AssetTypeDescriptor> types_;
};
inline AssetTypeRegistry& assetTypeRegistry() {
    static auto registry=[] {AssetTypeRegistry value;
        value.add({"model","Models",{".gltf",".glb"},true});value.add({"script","Scripts",{".lua"}});
        value.add({"managed-script","Managed scripts",{".azscript",".cs",".dll"}});value.add({"prefab","Prefabs",{".azureprefab"},true});
        value.add({"animation","Animation graphs / JSON",{".json"}});value.add({"audio","Audio",{".wav"}});
        value.add({"ui","UI documents",{".rml"}});value.add({"ui-style","UI styles",{".rcss"}});value.add({"level","Levels",{".azurelevel",".azscene"}});
        value.add({"image","Images",{".png",".jpg",".jpeg",".ppm",".hdr"}});value.add({"font","Fonts",{".ttf",".otf"}});
        value.add({"shader","Shaders",{".azshader",".slang",".glsl",".spv"}});value.add({"text","Text",{".txt",".md"}});
        return value;}();return registry;
}
}
