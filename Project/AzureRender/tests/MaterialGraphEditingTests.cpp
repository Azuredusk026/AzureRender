#include "editor/EditorSession.hpp"
#include "assets/MaterialGraph.hpp"
#include <chrono>
#include <iostream>
using namespace azurerender;
int main(){const auto root=std::filesystem::temp_directory_path()/("azure-graph-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
    Project::create(root,"Graph workflow");auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
    auto run=[&](const auto& op,const Json& args){const auto result=session.edit(op,args);if(!result)throw std::runtime_error(result.diagnostics.dump());return result.value;};
    auto check=[](bool condition,const char* message){if(!condition)throw std::runtime_error(message);};
    for(const auto* type:{"azure.pbr","azure.stylized"})for(const bool patch:{false,true}){
        const auto destination="assets:/"+std::string(type)+(patch?"-override":"")+".azurematerial";
        const auto asset=run("material.create",{{"type",type},{"mode",patch?"parameters":"replace"},{"destination",destination}}).at("asset").get<std::string>();
        const auto created=MaterialData::load(context->assets().resolve(asset));
        check(created.parameterOverride==patch,"Creation must preserve explicit mode");
        auto graph=MaterialGraph::fromMaterial(created);graph.setField("roughness",.2);
        const auto saved=run("material.graph",{{"asset",asset},{"graph",graph.encode()}}).at("document");
        run("material.field",{{"asset",asset},{"field","roughness"},{"value",.6}});
        auto edited=run("material.describe",{{"asset",asset}}).at("document");
        check(MaterialGraph::parse(edited.at("authoringGraph")).compile().parameters.at("roughness")==.6,"Form and graph must share committed values");
        if(patch)check(edited.at("parameters").size()==1&&edited.at("mode")=="parameters","Graph edits must preserve inherited fields");
        run("material.undo",{{"asset",asset}});check(run("material.describe",{{"asset",asset}}).at("document")==saved,"Undo must restore graph and compiled fields together");
        run("material.redo",{{"asset",asset}});check(run("material.describe",{{"asset",asset}}).at("document")==edited,"Redo must restore the complete graph transaction");
        auto bad=graph.encode();bad["links"][0]["source"]="missing";
        check(!session.edit("material.graph",{{"asset",asset},{"graph",bad}}),"Invalid graph must fail before publishing");
        check(MaterialData::load(context->assets().resolve(asset)).encode()==edited,"Rejected graph must preserve actual file bytes and values");
        auto reopened=EditorContext::openProject(root/"project.azureproject");EditorSession second(reopened);
        check(second.edit("material.describe",{{"asset",asset}}).value.at("document")==edited,"Reopening must restore graph layout and fields");
    }
    std::filesystem::remove_all(root);std::cout<<"Two material graph workflows, undo, redo, rejection and reopen passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
