#include "runtime/AssetDatabase.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
int main(){const auto root=std::filesystem::temp_directory_path()/("azure-2d-dependencies-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));const auto moved=root.string()+"-moved";try{
    Project::create(root,"2D dependencies");
    std::ofstream(root/"assets/image.svg")<<"<svg width=\"1\" height=\"1\"/>";
    std::ofstream(root/"assets/scene.azure2d")<<R"({"schemaVersion":1,"size":[640,360],"items":[{"id":"sprite","kind":"sprite","rect":[0,0,32,32],"image":"image.svg"}]})";
    AssetDatabase db(Project::load(root/"project.azureproject"));db.refresh();
    const auto scene=db.idForPath("assets:/scene.azure2d"),image=db.idForPath("assets:/image.svg");
    if(db.records().at(scene).dependencies!=std::vector<std::string>{image})throw std::runtime_error("2D image must be an asset dependency");
    const auto fingerprint=db.records().at(scene).fingerprint;
    std::ofstream(root/"assets/image.svg")<<"<svg width=\"2\" height=\"2\"/>";db.refresh(true);
    if(db.records().at(scene).fingerprint==fingerprint)throw std::runtime_error("Image changes must invalidate the scene");
    db.writePack(root/"pack");std::filesystem::rename(root,moved);
    const auto path=AssetDatabase::resolvePack(std::filesystem::path(moved)/"pack",scene);
    if(!std::filesystem::is_regular_file(path.parent_path()/"image.svg"))throw std::runtime_error("Moved pack must preserve relative image references");
    std::filesystem::remove(std::filesystem::path(moved)/"assets/image.svg");
    bool rejected=false;try{AssetDatabase broken(Project::load(std::filesystem::path(moved)/"project.azureproject"));broken.refresh();}catch(const std::exception&){rejected=true;}
    if(!rejected)throw std::runtime_error("Missing scene image must fail project validation");
    std::filesystem::remove_all(moved);std::cout<<"2D dependencies, invalidation, moved pack and missing image rejection passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);std::filesystem::remove_all(moved);return 1;}}
