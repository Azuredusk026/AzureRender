#include "runtime/ProjectReferenceViews.hpp"
#include "render/ReferenceCameraViewJson.hpp"
#include <atomic>
#include <chrono>
#include <fstream>
namespace azurerender {
namespace {
constexpr std::size_t maximumBytes=1024*1024;
void validateName(const std::string& name){
    if(name.empty()||name.size()>64)throw std::invalid_argument("Reference view name must contain1..64 ASCII characters");
    for(unsigned char c:name)if(!(c>='a'&&c<='z')&&!(c>='A'&&c<='Z')&&!(c>='0'&&c<='9')&&c!='.'&&c!='_'&&c!='-')throw std::invalid_argument("Invalid reference view name");
}
}
nlohmann::json encodeProjectReferenceViews(const ProjectReferenceViews& value){
    if(value.schemaVersion!=1||value.views.size()>64)throw std::invalid_argument("Unsupported project reference views version or count");
    auto views=nlohmann::json::object();for(const auto& [name,view]:value.views){validateName(name);views[name]=view;}
    return {{"schemaVersion",1},{"views",std::move(views)}};
}
ProjectReferenceViews decodeProjectReferenceViews(const nlohmann::json& j){
    if(!j.is_object()||j.size()!=2||!j.contains("schemaVersion")||!j.at("schemaVersion").is_number_integer()||j.at("schemaVersion")!=1||!j.contains("views")||!j.at("views").is_object()||j.at("views").size()>64)throw std::invalid_argument("Invalid project reference views document");
    ProjectReferenceViews result;for(const auto& entry:j.at("views").items()){validateName(entry.key());result.views.emplace(entry.key(),entry.value().get<ReferenceCameraView>());}return result;
}
std::filesystem::path projectReferenceViewsPath(const Project& project){
    if(project.file.empty())throw std::invalid_argument("Project reference views require a loaded project path");
    return project.file.parent_path()/"config/reference-views.json";
}
ProjectReferenceViews loadProjectReferenceViews(const Project& project){
    const auto path=projectReferenceViewsPath(project);if(!std::filesystem::exists(path))return {};
    if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>maximumBytes)throw std::invalid_argument("Project reference views file exceeds budget or is not a regular file");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot open project reference views");
    std::string data(maximumBytes+1,'\0');input.read(data.data(),static_cast<std::streamsize>(data.size()));const auto bytes=input.gcount();
    if(input.bad()||bytes>static_cast<std::streamsize>(maximumBytes))throw std::runtime_error("Cannot read bounded project reference views");data.resize(static_cast<std::size_t>(bytes));return decodeProjectReferenceViews(nlohmann::json::parse(data));
}
void saveProjectReferenceViews(const Project& project,const ProjectReferenceViews& value){
    const auto data=encodeProjectReferenceViews(value).dump(2)+'\n';if(data.size()>maximumBytes)throw std::invalid_argument("Project reference views exceed file budget");
    const auto path=projectReferenceViewsPath(project);std::filesystem::create_directories(path.parent_path());static std::atomic<std::uint64_t> sequence{0};
    auto temporary=path;temporary+=".tmp."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"."+std::to_string(sequence++);
    try{
        {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output<<data;output.flush();if(!output)throw std::runtime_error("Cannot save complete project reference views");}
        std::filesystem::rename(temporary,path);
    }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
}
