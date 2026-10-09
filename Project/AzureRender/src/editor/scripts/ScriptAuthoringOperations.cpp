#include "ScriptAuthoringRegistry.hpp"
#include "editor/EditorSession.hpp"
#include "runtime/AssetTypeRegistry.hpp"
namespace azurerender {
void registerScriptAuthoringOperations(EditRegistry& registry,EditorSession& session){
    using Json=nlohmann::json;const Json text={{"type","string"}};
    const auto schema=[&](Json fields,Json required=Json::array()){return Json{{"type","object"},{"properties",fields},{"required",required}};};
    registry.add({"script.backends",1,schema(Json::object()),false,false,false},[&session](EditorContext&,const Json&)->Json{return session.scriptAuthoring().describe();});
    registry.add({"script.template",1,schema({{"backend",text},{"path",text}},{"backend","path"}),false,false,true},[&session](EditorContext& c,const Json& a)->Json{
        session.contentDirectories().cancelRefresh();return session.scriptAuthoring().create(c,a.at("backend"),a.at("path"));
    });
    registry.add({"script.reveal",1,schema({{"asset",text}},{"asset"}),false,false,false},[&session](EditorContext& c,const Json& a)->Json{
        if(!c.isProject())throw EditRejection("Script reveal requires a project");const auto asset=a.at("asset").get<std::string>();const auto path=c.assets().resolveReference(asset);
        const auto* type=assetTypeRegistry().classify(path);if(!type||(type->id!="script"&&type->id!="managed-script"))throw EditRejection("Reveal expects a script asset");
        const auto result=session.edit("reference.reveal",{{"kind","asset"},{"id",asset}});if(!result)throw EditRejection(result.diagnostics.dump());return {{"path",path.u8string()},{"asset",asset}};
    });
    registry.add({"script.reload",1,schema({{"backend",text}},{"backend"}),false,false,false},[&session](EditorContext& c,const Json& a)->Json{
        const auto backend=a.at("backend").get<std::string>();if(session.building()||session.simulating()||!session.scriptAuthoring().contains(backend))throw EditRejection("Script reload requires a registered, available backend");
        if(auto* runtime=session.scripts()){if(runtime->capabilities().backend!=backend)throw EditRejection("Reload backend must match the active runtime");runtime->reloadChanged();return {{"runtime",true},{"errors",runtime->errors()}};}
        if(!c.isProject())throw EditRejection("Script reload requires a project");return {{"runtime",false},{"changedAssets",c.reloadChangedAssets()}};
    });
}
}
