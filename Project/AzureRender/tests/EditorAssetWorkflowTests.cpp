#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
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
        const auto formats=apply(session,"asset.formats");
        for(const std::string reader:{"gltf","glb","obj","stl","ply","dae","fbx"}) {
            bool found=false;for(const auto& format:formats)if(format.at("id")==reader)found=true;
            require(found,"Public format discovery must advertise each verified model reader");
        }
        for(const auto& format:formats)require(format.at("read").get<bool>()&&!format.at("write").get<bool>()&&format.at("settingsVersion")==1,"Public capabilities must report read/write and version separately");
        for(const auto& entry:catalog)if(entry.at("id")==id){++matches;require(entry.at("type")=="model"&&entry.at("ready").get<bool>(),"Catalog must classify actual resources through registration");}
        const auto models=apply(session,"assets.catalog",{{"type","model"}});
        std::size_t sceneOrder=models.size(),unusedOrder=models.size();for(std::size_t i=0;i<models.size();++i){if(models[i].at("id")==id)sceneOrder=i;if(models[i].at("path")=="assets:/unused.gltf")unusedOrder=i;}
        require(sceneOrder<unusedOrder,"Catalog ordering must prioritize scene resources independently of random UUID ordering");
        require(matches==1,"Virtual and absolute scene references must merge into one database identity");
        const auto importSource=root/std::filesystem::u8path(u8"导入 模型.GLTF");
        std::filesystem::copy_file(argv[1],importSource);
        const auto imported=apply(session,"asset.import",{{"path",importSource.u8string()}}).get<std::string>();
        require(!imported.empty(),"Public import must accept UTF-8 paths and uppercase registered extensions");
        const auto modelSource=root/std::filesystem::u8path(u8"转换 模型.OBJ");
        std::ofstream(modelSource)<<"v 0 0 0\nv 2 0 0\nv 0 3 0\nf 1 2 3\n";
        const auto converted=apply(session,"asset.import",{{"path",modelSource.u8string()}}).get<std::string>();
        require(context->assets().resolve(converted).extension()==".glb","Public model import must publish a runtime container");
        apply(session,"asset.place",{{"asset",converted},{"origin",{0,1,0}},{"direction",{0,-1,0}}});
        apply(session,"history.undo");apply(session,"history.redo");
        auto& authoredResources=context->scene().resources;
        authoredResources.erase(std::remove_if(authoredResources.begin(),authoredResources.end(),[](const auto& resource){return resource.id=="virtual-alias"||resource.id=="absolute-alias";}),authoredResources.end());
        context->save();
        const auto fingerprint=std::to_string(context->assets().records().at(converted).fingerprint);
        apply(session,"asset.reimport",{{"asset",converted},{"settings",Json::object()},{"baseFingerprint",fingerprint}});
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(std::chrono::steady_clock::now()<deadline){session.pollTasks();if(session.tasks().report().back().at("state")!="running")break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        require(session.tasks().report().back().at("state")=="completed","Converted models must preserve material mapping through production reimport");
        std::filesystem::remove(modelSource);
        const auto pack=root/std::filesystem::u8path(u8"移动资源包");context->assets().writePack(pack);
        require(std::filesystem::is_regular_file(AssetDatabase::resolvePack(pack,imported)),"UTF-8 imported names must survive portable resource packing");
        require(std::filesystem::is_regular_file(AssetDatabase::resolvePack(pack,converted)),"Converted provenance and embedded data must remain portable after source removal");
        // Catalog-only aliases above have no authored portable references.
        auto& resources=context->scene().resources;
        resources.erase(std::remove_if(resources.begin(),resources.end(),[](const auto& resource){return resource.id=="virtual-alias"||resource.id=="absolute-alias";}),resources.end());
        context->save();
        const auto reloaded=EditorContext::openProject(root/"project.azureproject");
        require(reloaded->assets().resolve(imported)==context->assets().resolve(imported),"Imported UTF-8 model identity must survive save and reopen");
        require(reloaded->assets().resolve(converted)==context->assets().resolve(converted),"Converted asset references must survive save and reopen");
        apply(session,"asset.place",{{"asset",imported},{"origin",{2,4,3}},{"direction",{0,-1,0}}});
        apply(session,"history.undo");
        const auto detachedModel=root/"assets"/std::filesystem::u8path(u8"放置 模型.GLTF");
        std::filesystem::copy_file(argv[1],detachedModel);context->assets().refresh();
        const auto detached=context->assets().idForPath(u8"assets:/放置 模型.GLTF");
        apply(session,"asset.place",{{"asset",detached},{"origin",{2,4,3}},{"direction",{0,-1,0}}});
        apply(session,"history.undo");
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
