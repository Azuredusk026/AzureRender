#include "PrefabAuthoringService.hpp"
#include "editor/EditorSession.hpp"
namespace azurerender {
void registerPrefabAuthoringOperations(EditRegistry& registry,EditorSession& session){
    using Json=nlohmann::json;const Json text={{"type","string"}};
    const Json field={{"type","object"},{"properties",{{"node",text},{"path",text}}},{"required",{"node","path"}}};
    const Json fields={{"type","array"},{"minItems",1},{"maxItems",128},{"items",field}};
    const auto schema=[](Json properties,Json required){return Json{{"type","object"},{"properties",properties},{"required",required}};};
    registry.add({"prefab.create",1,schema({{"nodes",{{"type","array"},{"minItems",1},{"maxItems",256},{"items",text}}},{"path",text}},{"nodes","path"}),false,false,true},[&session](EditorContext&,const Json& a){return session.prefabs().create(session,a.at("nodes").get<std::vector<std::string>>(),a.at("path"));});
    registry.add({"prefab.overrides",1,schema({{"instance",text}},{"instance"}),false,false,false},[&session](EditorContext& c,const Json& a){return session.prefabs().overrides(c,a.at("instance"));});
    registry.add({"prefab.revert",1,schema({{"instance",text},{"fields",fields}},{"instance","fields"}),true,true,true},[&session](EditorContext& c,const Json& a)->Json{session.prefabs().revert(c,a.at("instance"),a.at("fields"));return nullptr;});
    registry.add({"prefab.apply",1,schema({{"instance",text},{"fields",fields},{"baseFingerprint",text}},{"instance","fields","baseFingerprint"}),true,false,true},[&session](EditorContext&,const Json& a){return session.prefabs().apply(session,a.at("instance"),a.at("fields"),a.at("baseFingerprint"));});
    registry.add({"prefab.undo-apply",1,schema(Json::object(),Json::array()),true,false,true},[&session](EditorContext&,const Json&)->Json{session.prefabs().undoApply(session);return nullptr;});
    registry.add({"prefab.redo-apply",1,schema(Json::object(),Json::array()),true,false,true},[&session](EditorContext&,const Json&)->Json{session.prefabs().redoApply(session);return nullptr;});
}
}
