#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){if(argc!=2)return 2;try{
    const auto root=std::filesystem::temp_directory_path()/("azure-authoring-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::copy(argv[1],root,std::filesystem::copy_options::recursive);
    auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
    const auto original=context->documentContent();
    require(static_cast<bool>(session.edit("script.template",{{"backend","lua"},{"path","assets:/Controller.lua"}})),"Create Lua script through its registered backend");
    require(static_cast<bool>(session.edit("script.template",{{"backend","coreclr"},{"path","assets:/Controller.cs"}})),"Create managed script through its registered backend");
    require(std::filesystem::is_regular_file(root/"assets/Controller.lua")&&std::filesystem::is_regular_file(root/"assets/Controller.cs"),"Both templates install sources");
    const auto id=context->assets().idForPath("assets:/Controller.lua");require(!id.empty(),"Template source enters the asset database");
    require(!session.edit("script.template",{{"backend","lua"},{"path","assets:/Controller.lua"}}),"Existing script is protected");
    require(!session.edit("script.template",{{"backend","missing"},{"path","assets:/Other.lua"}}),"Unregistered backend is rejected");
    require(!session.edit("script.template",{{"backend","lua"},{"path","assets:/../outside.lua"}}),"Template path stays in project mount");
    require(!session.edit("script.template",{{"backend","lua"},{"path","assets:/Wrong.cs"}}),"Template extension belongs to backend");
    require(!session.edit("script.template",{{"backend","coreclr"},{"path","assets:/9Invalid.cs"}}),"Managed source validates its class identifier");
    require(static_cast<bool>(session.edit("script.reveal",{{"asset",context->assets().idForPath("assets:/Controller.cs")}})),"Managed source is revealable through the registered asset type");
    require(context->documentContent()==original&&!context->dirty(),"Script source creation leaves scene content intact");
    std::ifstream templateInput(root/"assets/Controller.lua");std::string templateSource{std::istreambuf_iterator<char>(templateInput),{}};templateInput.close();
    const auto init=templateSource.find("function init()\nend");require(init!=std::string::npos,"Lua template exports the production global init callback");
    templateSource.replace(init,std::string("function init()\nend").size(),"function init()\n    self:set('azure.transform','translation',{7,0,0})\nend");
    require(static_cast<bool>(session.edit("script.write",{{"path","assets:/Controller.lua"},{"value",templateSource}})),"Edit generated source through the public script operation");
    require(static_cast<bool>(session.edit("component.add",{{"type","azure.script"}})),"Attach the Lua template");
    require(static_cast<bool>(session.edit("component.field",{{"type","azure.script"},{"field","asset"},{"value",id}})),"Use shared reference metadata");
    std::size_t expectedScripts=0;for(const auto& [node,components]:context->runtimeComponents())if(components.contains("azure.script")&&components.at("azure.script").at("data").value("enabled",true)&&!components.at("azure.script").at("data").at("asset").get<std::string>().empty())++expectedScripts;
    require(session.execute(EditorCommand::Play),"Play project containing template");session.advance(.05);
    if(session.scripts())for(const auto& error:session.scripts()->errors())std::cerr<<error<<'\n';
    require(session.scripts()&&session.scripts()->activeCount()==expectedScripts&&session.scripts()->errors().empty(),"Created Lua template and existing inspector scripts run through production backend");
    require(session.runtime()->world().tryGet<ecs::TransformComponent>(session.runtime()->entity(context->selectedNode()->id))->translation[0]==7,"Template initialization executes through the production callback");
    require(static_cast<bool>(session.edit("script.reload",{{"backend","lua"}})),"Reload active registered backend");
    require(!session.edit("script.reload",{{"backend","coreclr"}}),"Reload checks runtime backend identity");
    require(session.execute(EditorCommand::Stop),"Stop template project");
    const auto errorNode=context->selectedNode()->id,firstDocument=context->documentId();
    require(static_cast<bool>(session.edit("script.write",{{"path","assets:/Controller.lua"},{"value","function update(delta) error('Authoring failure') end\n"}})),"Install a callback failure fixture");
    require(session.execute(EditorCommand::Play),"Start runtime diagnostics fixture");session.advance(.05);
    auto runtimeProblems=session.edit("diagnostic.query",{{"source","script.lua"}});
    require(static_cast<bool>(runtimeProblems)&&!runtimeProblems.value.at("entries").empty(),"Script failures enter structured problems");
    const auto runtimeProblem=runtimeProblems.value.at("entries").back();
    require(runtimeProblem.at("node")==errorNode&&!runtimeProblem.at("asset").get<std::string>().empty()&&runtimeProblem.at("line").get<unsigned>()==1,"Runtime diagnostics preserve exact object, source and line");
    require(session.execute(EditorCommand::Stop),"Stop diagnostic fixture");
    require(static_cast<bool>(session.edit("document.save")),"Save first diagnostic scene");
    std::filesystem::copy_file(context->scenePath(),root/"assets/second.azurelevel");context->assets().refresh();
    require(static_cast<bool>(session.edit("document.open",{{"path","assets:/second.azurelevel"}})),"Open same object identifiers in another document");session.pollTasks();
    require(context->documentId()!=session.context().documentId(),"Diagnostic location has a document identity");
    require(static_cast<bool>(session.edit("diagnostic.reveal",{{"id",runtimeProblem.at("id")}})),"Reveal runtime failure across documents");session.pollTasks();
    require(session.context().documentId()==firstDocument&&session.context().selectedNode()->id==errorNode,"Problem reveal selects exact owning document and node");
    auto query=session.edit("diagnostic.query",{{"source","script.template"}});
    require(static_cast<bool>(query)&&query.value.at("entries").size()>=4,"Structured problems are queryable by source");
    const auto diagnostic=query.value.at("entries").front().at("id");
    require(static_cast<bool>(session.edit("diagnostic.reveal",{{"id",diagnostic}})),"Problem reveals its owning document");session.pollTasks();
    require(!session.edit("diagnostic.reveal",{{"id","missing"}}),"Unknown problem is rejected");
    session.setDeveloperServices({{},[]{return nlohmann::json{{"shaderDiagnostic","Invalid shader fixture"}};},{}});session.pollTasks();session.pollTasks();
    query=session.edit("diagnostic.query",{{"source","shader.compile"}});require(query.value.at("entries").size()==1&&query.value.at("entries")[0].at("code")=="CompilationFailed","Shader compiler failures use structured bounded diagnostics without repeated frame duplicates");
    require(!session.edit("asset.import",{{"path",(root/"missing.gltf").u8string()}}),"Import failure fixture uses public operation");
    query=session.edit("diagnostic.query",{{"source","asset.import"}});
    require(!query.value.at("entries").empty()&&query.value.at("entries").back().at("path")== (root/"missing.gltf").u8string(),"Import diagnostics preserve the exact input path");
    for(unsigned i=0;i<530;++i)session.edit("missing-"+std::to_string(i));
    query=session.edit("diagnostic.query");require(query.value.at("capacity")==512&&query.value.at("entries").size()==512&&query.value.at("dropped").get<unsigned>()>0,"Problem storage is bounded and reports dropped entries");
    std::filesystem::remove_all(root);std::cout<<"Registered script authoring and structured problem workflows passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
