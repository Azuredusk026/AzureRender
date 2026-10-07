#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json apply(EditorSession& session,const std::string& id,Json parameters=Json::object()){
    const auto result=session.edit(id,std::move(parameters));if(!result)throw std::runtime_error(id+": "+result.diagnostics.dump());return result.value;
}
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const auto root=std::filesystem::temp_directory_path()/("azure-assets-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));int status=0;
    try{
        Project::create(root,"Asset workflow");
        std::filesystem::copy_file(argv[1],root/"assets/model.gltf");
        std::filesystem::copy_file(argv[1],root/"assets/unused.gltf");
        std::ofstream(root/"assets/model.gltf.azmeta")<<Json{{"schemaVersion",1},{"id","ffffffff-ffff-4fff-8fff-ffffffffffff"},{"settings",Json::object()},{"dependencies",Json::array()}}.dump();
        std::ofstream(root/"assets/unused.gltf.azmeta")<<Json{{"schemaVersion",1},{"id","00000000-0000-4000-8000-000000000000"},{"settings",Json::object()},{"dependencies",Json::array()}}.dump();
        auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
        const auto id=context->assets().idForPath("assets:/model.gltf");
        context->scene().resources.push_back({"virtual-alias","gltf",std::filesystem::u8path("assets:/model.gltf")});
        context->scene().resources.push_back({"absolute-alias","gltf",root/"assets/model.gltf"});
        const auto catalog=apply(session,"assets.catalog");unsigned matches=0;
        for(const auto& entry:catalog)if(entry.at("id")==id){++matches;require(entry.at("type")=="model"&&entry.at("ready").get<bool>(),"Catalog must classify actual resources through registration");}
        const auto models=apply(session,"assets.catalog",{{"type","model"}});
        std::size_t sceneOrder=models.size(),unusedOrder=models.size();for(std::size_t i=0;i<models.size();++i){if(models[i].at("id")==id)sceneOrder=i;if(models[i].at("path")=="assets:/unused.gltf")unusedOrder=i;}
        require(sceneOrder<unusedOrder,"Catalog ordering must prioritize scene resources independently of random UUID ordering");
        require(matches==1,"Virtual and absolute scene references must merge into one database identity");
        const auto before=context->scene().nodes.size();const auto undo=context->undoCount();
        apply(session,"asset.place",{{"asset",id},{"origin",{2,4,3}},{"direction",{0,-1,0}}});
        require(context->scene().nodes.size()==before+1&&context->selectedNode()->translation==std::array<float,3>{2,0,3},"Ray placement must intersect the shared fallback plane");
        require(context->undoCount()==undo+1,"Placement and transform must form one undo unit");
        apply(session,"history.undo");require(context->scene().nodes.size()==before,"Placement undo must remove the placed node");
        apply(session,"asset.place",{{"asset",id},{"origin",{2,4,3}},{"direction",{0,0,-1}},{"fallbackDistance",5}});
        require(context->selectedNode()->translation==std::array<float,3>{2,4,-2},"Parallel rays must use the declared positive fallback distance");
        const auto authored=context->documentContent();
        require(!session.edit("asset.place",{{"asset",id},{"origin",{2,4,3}},{"direction",{0,0,0}}}),"Placement must reject zero-length directions");
        require(context->documentContent()==authored,"Rejected ray must preserve content");
        const auto directory=root/std::filesystem::u8path(u8"中文路径")/std::string(100,'x');std::filesystem::create_directories(directory);
        apply(session,"path.remember",{{"purpose","import"},{"path",directory.u8string()},{"directory",true}});
        const auto history=apply(session,"path.history",{{"purpose","import"}});
        require(history.size()==1&&history[0]==directory.u8string(),"Path history must preserve long UTF-8 directory names");
        apply(session,"path.remember",{{"purpose","import"},{"path",directory.u8string()},{"directory",true}});
        require(apply(session,"path.history",{{"purpose","import"}}).size()==1,"Recent-directory history must deduplicate identities");
        const auto saveProject=root/"save-project";Project::create(saveProject,"Save contract");
        Json descriptor;{std::ifstream input(saveProject/"project.azureproject");input>>descriptor;}
        descriptor["startupScene"]="assets:/save.azurelevel";std::ofstream(saveProject/"project.azureproject")<<descriptor.dump();
        std::ofstream(saveProject/"assets/save.azurelevel")<<Json{{"schemaVersion",1},{"id","save"},{"sceneType","sample"},{"resources",Json::array()},
            {"nodes",Json::array({{{"id","a"},{"components",Json::object()}},{{"id","b"},{"components",Json::object()}}})}}.dump();
        auto saved=EditorContext::openProject(saveProject/"project.azureproject");EditorSession saving(saved);
        apply(saving,"node.reparent",{{"id","a"},{"parent","b"}});const auto saveContent=saved->documentContent();
        apply(saving,"document.save");require(saved->documentContent()==saveContent,"Saving a new hierarchy must preserve the authored document fingerprint");
        std::cout<<"Asset identity, ray placement, undo and UTF-8 directory history passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
