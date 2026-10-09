#include "ProjectOpenService.hpp"
#include "editor/commands/DocumentVersion.hpp"
#include <fstream>
namespace azurerender {
ProjectOpenService::ProjectOpenService(std::filesystem::path history):history_(std::move(history)) {
    try {
        if(!std::filesystem::exists(history_))return;
        if(std::filesystem::file_size(history_)>65536)throw std::runtime_error("Recent project budget exceeded");
        std::ifstream input(history_);nlohmann::json document;input>>document;
        if(document.at("schemaVersion")!=1||!document.at("projects").is_array()||document.at("projects").size()>16)
            throw std::runtime_error("Invalid project history");
        for(const auto& record:document.at("projects")) {
            record.at("path").get<std::string>();record.at("name").get<std::string>();record.at("id").get<std::string>();
        }
        records_=document.at("projects");
    }catch(const std::exception&){records_=nlohmann::json::array();}
}
nlohmann::json ProjectOpenService::templates() {
    return nlohmann::json::array({{{"id","scene"},{"label","Scene inspection"}},
        {{"id","game"},{"label","Playable game"}}});
}
std::shared_ptr<EditorContext> ProjectOpenService::open(const std::filesystem::path& path) const {
    if(path.extension()!=".azureproject")throw std::invalid_argument("Select a .azureproject document");
    return EditorContext::openProject(path);
}
std::shared_ptr<EditorContext> ProjectOpenService::create(const std::string& templateId,
    const std::filesystem::path& destination,const std::string& name,const std::function<bool()>& cancelled) {
    if(templateId!="scene"&&templateId!="game")throw std::invalid_argument("Unknown project template");
    if(destination.empty()||name.empty()||name.size()>1024)throw std::invalid_argument("Project destination and name are required");
    const auto target=std::filesystem::absolute(destination).lexically_normal();
    if(std::filesystem::exists(target))throw std::invalid_argument("Project destination already exists");
    const auto temporary=target.parent_path()/(".azure-project-"+newDocumentIdentity());
    try {
        if(cancelled&&cancelled())throw std::runtime_error("Project creation cancelled");
        if(templateId=="scene")Project::create(temporary,name);else Project::createGame(temporary,name);
        static_cast<void>(open(temporary/"project.azureproject"));
        if(cancelled&&cancelled())throw std::runtime_error("Project creation cancelled");
        std::filesystem::rename(temporary,target);
    }catch(...){std::filesystem::remove_all(temporary);throw;}
    return open(target/"project.azureproject");
}
void ProjectOpenService::remember(const Project& project) {
    auto candidate=nlohmann::json::array();
    candidate.push_back({{"id",project.id},{"name",project.name},{"path",project.file.u8string()}});
    for(const auto& record:records_)if(record.at("path")!=project.file.u8string()&&candidate.size()<16)candidate.push_back(record);
    std::filesystem::create_directories(history_.parent_path());
    const auto pending=history_.parent_path()/("recent-"+newDocumentIdentity()+".pending");
    try {
        {std::ofstream output(pending);output<<nlohmann::json{{"schemaVersion",1},{"projects",candidate}}.dump(2);output.flush();
            if(!output)throw std::runtime_error("Cannot persist recent projects");}
        std::filesystem::copy_file(pending,history_,std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(pending);records_=std::move(candidate);
    }catch(...){std::filesystem::remove(pending);throw;}
}
nlohmann::json ProjectOpenService::recent() const {
    auto result=records_;
    for(auto& record:result){
        try{static_cast<void>(Project::load(std::filesystem::u8path(record.at("path").get<std::string>())));record["available"]=true;}
        catch(const std::exception& error){record["available"]=false;record["diagnostic"]=error.what();}
    }
    return result;
}
}
