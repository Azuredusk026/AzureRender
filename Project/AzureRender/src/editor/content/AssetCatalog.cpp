#include "AssetCatalog.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include <map>
#include <algorithm>
namespace azurerender {
nlohmann::json assetCatalog(const EditorContext& context,const std::string& filter) {
    std::map<std::string,nlohmann::json> rows;
    const auto key=[](const std::filesystem::path& path){std::error_code error;auto value=std::filesystem::weakly_canonical(path,error);return (error?path.lexically_normal():value).generic_u8string();};
    std::map<std::string,std::string> identities;
    if(context.isProject())for(const auto& [id,record]:context.assets().records()){
        identities[key(record.path)]=id;const auto* type=assetTypeRegistry().classify(record.path);
        rows[id]={{"id",id},{"path",record.virtualPath},{"source",record.path.u8string()},{"type",type?type->id:"unknown"},{"ready",std::filesystem::is_regular_file(record.path)},{"users",0},{"resources",nlohmann::json::array()}};
    }
    std::map<std::string,std::size_t> sceneRanks;std::size_t rank=0;
    for(const auto& resource:context.resourceStatuses()){
        auto path=resource.path;if(context.isProject())try{if(path.generic_u8string().find(":/")!=std::string::npos)path=context.assets().resolveReference(path.generic_u8string());}catch(const std::exception&){}
        const auto found=identities.find(key(path));const auto id=found==identities.end()?resource.id:found->second;
        if(!sceneRanks.count(id))sceneRanks[id]=rank++;
        if(!rows.count(id)){const auto* type=assetTypeRegistry().classify(path);rows[id]={{"id",id},{"path",path.filename().u8string()},{"source",path.u8string()},{"type",type?type->id:"unknown"},{"ready",std::filesystem::is_regular_file(path)},{"users",0},{"resources",nlohmann::json::array()}};}
        rows[id]["users"]=rows[id]["users"].get<std::size_t>()+resource.dependentNodeCount;rows[id]["resources"].push_back(resource.id);
    }
    auto result=nlohmann::json::array();for(const auto& [id,row]:rows)if(filter.empty()||row.at("type")==filter)result.push_back(row);
    std::sort(result.begin(),result.end(),[&](const auto& left,const auto& right){
        const auto a=sceneRanks.find(left.at("id").template get<std::string>()),b=sceneRanks.find(right.at("id").template get<std::string>());
        if((a==sceneRanks.end())!=(b==sceneRanks.end()))return a!=sceneRanks.end();
        if(a!=sceneRanks.end()&&a->second!=b->second)return a->second<b->second;
        return left.at("path").template get<std::string>()<right.at("path").template get<std::string>();
    });return result;
}
}
