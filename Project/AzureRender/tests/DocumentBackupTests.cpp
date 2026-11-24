#include "editor/EditorContext.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
int main(){
    using namespace azurerender;
    const auto root=std::filesystem::temp_directory_path()/("azure-document-backup-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    int result=0;
    try {
        EditorContext context(SceneDocument::fromAsset("mesh.gltf"),root/"scene.azscene");
        context.save();
        const auto savedName=context.selectedNode()->name;
        context.setSelectedNodeName("New authored content");context.save();
        if(!std::filesystem::is_regular_file(context.recoveryPath())){std::cerr<<"Save must preserve a separately named recovery document\n";result=1;}
        else {
            const auto recovered=SceneDocument::load(context.recoveryPath());
            if(recovered.nodes.front().name!=savedName || SceneDocument::load(context.scenePath()).nodes.front().name!="New authored content")result=2;
            std::ifstream input(context.recoveryPath().string()+".json");nlohmann::json record;input>>record;
            if(record.at("size").get<std::uintmax_t>()!=std::filesystem::file_size(context.recoveryPath()))result=3;
        }
        Project::create(root/"project", "Recovery");
        auto projectContext=EditorContext::openProject(root/"project/project.azureproject");
        const auto records=projectContext->assets().records().size();
        projectContext->setSelectedNodeName("Project recovery content");projectContext->save();
        if(!std::filesystem::is_regular_file(projectContext->recoveryPath())
            || projectContext->recoveryPath().parent_path()!=root/"project/.azure/recovery"
            || projectContext->assets().records().size()!=records)result=5;
        const auto previous=SceneDocument::load(projectContext->recoveryPath());
        if(previous.nodes.front().name=="Project recovery content")result=6;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';result=4;}
    std::filesystem::remove_all(root);return result;
}
