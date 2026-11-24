#pragma once
#include "editor/EditorContext.hpp"
#include "runtime/AssetTypeRegistry.hpp"
namespace azurerender {
// Provenance and model bindings are read-only. Component references use the picker.
inline nlohmann::json objectReferences(const EditorContext& context,const SceneNode& node) {
    auto result=nlohmann::json::array();
    const auto asset=[&](const char* label,const std::string& id,const std::vector<std::string>& types) {
        if(id.empty())return;
        bool valid=false;std::string path,message;
        try{
            const auto resolved=context.assets().resolveReference(id);
            valid=std::filesystem::is_regular_file(resolved)&&assetTypeRegistry().matches(resolved,types);
            path=resolved.u8string();if(!valid)message="来源资产类型不兼容或文件缺失";
        }catch(const std::exception& error){message=error.what();}
        result.push_back({{"label",label},{"kind","asset"},{"id",id},{"path",path},{"valid",valid},{"message",message}});
    };
    asset("预制体来源",node.prefabSource,{"prefab"});
    if(!node.resourceId.empty()){
        auto id=node.resourceId;
        for(const auto& resource:context.resourceStatuses())if(resource.id==id){
            for(const auto& record:context.assets().records())if(record.second.path==resource.path){id=record.first;break;}
            break;
        }
        asset("模型资源",id,{"model"});
    }
    return result;
}
}
