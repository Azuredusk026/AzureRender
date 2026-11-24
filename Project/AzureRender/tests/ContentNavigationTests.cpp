#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
Json run(EditorSession& session,const std::string& command,Json args=Json::object()){
    const auto result=session.edit(command,args);if(!result)throw std::runtime_error(command+": "+result.diagnostics.dump());return result.value;
}
}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("azure-navigation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto relocated=std::filesystem::path(root.u8string()+"-moved");
    int status=0;
    try{
        Project::create(root,"Navigation");std::filesystem::create_directories(root/"assets/empty");
        std::filesystem::create_directories(root/"assets/nested/sub");
        std::ofstream(root/"assets/nested/readme.txt")<<"hello";
        auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
        const auto before=context->documentContent();
        const auto empty=run(session,"content.directory",{{"mount","assets"},{"path","empty"}});
        require(empty.at("assets").empty()&&empty.at("directories").empty(),"Empty directories must be navigable");
        const auto folder=run(session,"content.directory",{{"mount","assets"},{"path","nested"}});
        require(folder.at("assets").size()==1&&folder.at("directories").size()==1,"Folder snapshot must include only direct children");
        const auto version=folder.at("version");
        const auto cached=run(session,"content.directory",{{"mount","assets"},{"path","nested"}});
        require(cached.at("version")==version&&cached.at("statistics").at("catalogBuilds")==folder.at("statistics").at("catalogBuilds"),"Unchanged views must reuse the catalog");
        const auto id=context->assets().idForPath("assets:/nested/readme.txt");
        const auto opened=run(session,"asset.open",{{"asset",id}});
        require(opened.at("operation")=="asset.inspect"&&opened.at("asset")==id,"Registered text opening must return the read-only target");
        require(context->documentContent()==before&&!context->dirty(),"Browsing and opening must preserve scene content");
        run(session,"asset.inspect",{{"asset",id}});
        require(!session.edit("content.directory",{{"mount","assets"},{"path","../"}}),"Navigation must reject mount escape");
        require(!session.edit("content.directory",{{"mount","missing"}}),"Unknown mounts must be rejected");
        require(!session.edit("asset.open",{{"asset","missing"}}),"Missing assets must report failure");
        std::filesystem::create_directories(root/"assets/nested/external");
        std::ofstream(root/"assets/nested/external/new.txt")<<"new";
        const auto external=run(session,"content.directory",{{"mount","assets"},{"path","nested/external"},{"refresh",true}});
        require(external.at("assets").size()==1&&external.at("version")!=version,"External directory changes must publish a new snapshot");
        const auto externalId=context->assets().idForPath("assets:/nested/external/new.txt");
        require(run(session,"asset.inspect",{{"asset",externalId}}).at("id")==externalId,"Existing content services must discover externally registered assets");
        require(context->documentContent()==before&&!context->dirty(),"External discovery must preserve authored scene");
        const Json generation={{"generator","azure.primitive"},{"output","assets:/nested/shape.glb"},{"parameters",{{"kind","cube"},{"size",{2,3,4}},{"segments",2}}},{"license","Azure primitive generator v1"},{"replace",false}};
        const auto generated=run(session,"asset.generate",generation).get<std::string>();
        const auto inspected=run(session,"asset.inspect",{{"asset",generated}});
        require(inspected.at("generation").at("generatorId")=="azure.primitive","Generated standard assets must expose provenance through the common service");
        require(!session.edit("asset.generate",generation),"Create-only generation must reject overwriting an existing source");
        const auto presets=run(session,"node.presets");
        require(presets.size()>=2,"Camera and light presets must be discoverable");
        const auto undo=context->undoCount(),nodeCount=context->scene().nodes.size();
        run(session,"node.create-preset",{{"preset","light"},{"id","work-light"}});
        require(context->scene().lights.size()==1&&context->scene().lights[0].nodeId=="work-light","Light preset must attach an actual emitter");
        require(context->undoCount()==undo+1,"Preset creation must form one undo unit");
        run(session,"history.undo");require(context->scene().lights.empty()&&context->scene().nodes.size()==nodeCount,"Undo must remove the light and its owning node");
        run(session,"node.create-preset",{{"preset","camera"},{"id","work-camera"}});
        require(!context->componentData("work-camera","azure.camera").is_null(),"Camera preset must instantiate a registered camera component");
        const auto cameraUndo=context->undoCount();
        run(session,"viewport.camera-preview",{{"node","work-camera"}});
        require(session.viewportView().position==std::array<float,3>{0,0,0}&&session.viewportView().target==std::array<float,3>{0,0,-1},"Camera preview uses the authored camera pose");
        require(context->undoCount()==cameraUndo,"Camera preview does not mutate the scene document");
        run(session,"history.undo");
        std::filesystem::copy(root,relocated,std::filesystem::copy_options::recursive);
        auto movedContext=EditorContext::openProject(relocated/"project.azureproject");EditorSession moved(movedContext);
        const auto movedSnapshot=run(moved,"content.directory",{{"mount","assets"},{"path","nested"},{"type","text"}});
        require(movedSnapshot.at("assets").size()==1&&movedSnapshot.at("assets")[0].at("id")==id,"Project relocation must preserve navigation and asset identities");
        std::cout<<"Empty folders, scoped snapshots, cache reuse, external changes and registered asset opening passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);std::filesystem::remove_all(relocated);return status;
}
