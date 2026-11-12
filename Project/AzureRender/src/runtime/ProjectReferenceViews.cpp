#include "runtime/ProjectReferenceViews.hpp"
#include "render/ReferenceCameraViewJson.hpp"
#include <algorithm>
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
constexpr const char* visibilityArithmeticField="objectVisibilityArithmetic";
void validateVisibilityArithmetic(const ProjectReferenceViews& value){
    if(value.objectVisibilityArithmetic==ScreenObjectVisibilityArithmetic::Device)return;
    if(value.objectVisibilityArithmetic!=ScreenObjectVisibilityArithmetic::ProfileV1)
        throw std::invalid_argument("Unsupported project object visibility arithmetic");
    if(std::none_of(value.views.begin(),value.views.end(),[](const auto& entry){return entry.second.objectVisibility.has_value();}))
        throw std::invalid_argument("Project visibility profile requires a view with object visibility");
}
std::shared_ptr<const ScreenObjectVisibilityNumerics> loadVisibilityProfile(const Project& project){
    const auto path=projectReferenceVisibilityProfilePath(project);
    if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)!=kScreenObjectVisibilityProfileBytes)
        throw std::invalid_argument("Project visibility profile requires exactly 2097152 bytes");
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot open project visibility profile");
    std::vector<char> bytes(kScreenObjectVisibilityProfileBytes);
    const auto length=static_cast<std::streamsize>(bytes.size());
    input.read(bytes.data(),length);
    if(input.gcount()!=length||input.peek()!=EOF||input.bad())throw std::runtime_error("Cannot read complete project visibility profile");
    const auto byte=[&](std::size_t offset){return static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[offset]));};
    auto profile=std::make_shared<ScreenObjectVisibilityNumerics>();
    profile->mode=ScreenObjectVisibilityArithmetic::ProfileV1;
    profile->reciprocalCorrections.resize(kScreenObjectVisibilityProfileWords);
    for(std::size_t index=0;index<profile->reciprocalCorrections.size();++index){
        const auto offset=index*4;
        const auto word=byte(offset)|(byte(offset+1)<<8)|(byte(offset+2)<<16)|(byte(offset+3)<<24);
        if(!validScreenObjectVisibilityProfileWord(word))throw std::invalid_argument("Project visibility profile contains a reserved code");
        profile->reciprocalCorrections[index]=word;
    }
    return profile;
}
}
nlohmann::json encodeProjectReferenceViews(const ProjectReferenceViews& value){
    if(value.schemaVersion!=1||value.views.size()>64)throw std::invalid_argument("Unsupported project reference views version or count");
    validateVisibilityArithmetic(value);
    auto views=nlohmann::json::object();for(const auto& [name,view]:value.views){validateName(name);views[name]=view;}
    nlohmann::json document={{"schemaVersion",1},{"views",std::move(views)}};
    if(value.objectVisibilityArithmetic==ScreenObjectVisibilityArithmetic::ProfileV1)document[visibilityArithmeticField]="profile-v1";
    return document;
}
ProjectReferenceViews decodeProjectReferenceViews(const nlohmann::json& j){
    const bool profile=j.contains(visibilityArithmeticField);
    if(!j.is_object()||j.size()!=(profile?3U:2U)||!j.contains("schemaVersion")||!j.at("schemaVersion").is_number_integer()||j.at("schemaVersion")!=1||!j.contains("views")||!j.at("views").is_object()||j.at("views").size()>64)throw std::invalid_argument("Invalid project reference views document");
    ProjectReferenceViews result;
    if(profile){
        if(j.at(visibilityArithmeticField)!="profile-v1")throw std::invalid_argument("Unsupported project object visibility arithmetic");
        result.objectVisibilityArithmetic=ScreenObjectVisibilityArithmetic::ProfileV1;
    }
    for(const auto& entry:j.at("views").items()){validateName(entry.key());result.views.emplace(entry.key(),entry.value().get<ReferenceCameraView>());}
    validateVisibilityArithmetic(result);
    return result;
}
std::filesystem::path projectReferenceViewsPath(const Project& project){
    if(project.file.empty())throw std::invalid_argument("Project reference views require a loaded project path");
    return project.file.parent_path()/"config/reference-views.json";
}
std::filesystem::path projectReferenceVisibilityProfilePath(const Project& project){
    return projectReferenceViewsPath(project).parent_path()/"object-visibility-profile-v1.bin";
}
ProjectReferenceViews loadProjectReferenceViews(const Project& project){
    const auto path=projectReferenceViewsPath(project);if(!std::filesystem::exists(path))return {};
    if(!std::filesystem::is_regular_file(path)||std::filesystem::file_size(path)>maximumBytes)throw std::invalid_argument("Project reference views file exceeds budget or is not a regular file");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot open project reference views");
    std::string data(maximumBytes+1,'\0');input.read(data.data(),static_cast<std::streamsize>(data.size()));const auto bytes=input.gcount();
    if(input.bad()||bytes>static_cast<std::streamsize>(maximumBytes))throw std::runtime_error("Cannot read bounded project reference views");data.resize(static_cast<std::size_t>(bytes));
    auto result=decodeProjectReferenceViews(nlohmann::json::parse(data));
    if(result.objectVisibilityArithmetic==ScreenObjectVisibilityArithmetic::ProfileV1)result.objectVisibilityProfile=loadVisibilityProfile(project);
    return result;
}
void saveProjectReferenceViews(const Project& project,const ProjectReferenceViews& value){
    const auto data=encodeProjectReferenceViews(value).dump(2)+'\n';if(data.size()>maximumBytes)throw std::invalid_argument("Project reference views exceed file budget");
    if(value.objectVisibilityArithmetic==ScreenObjectVisibilityArithmetic::ProfileV1)(void)loadVisibilityProfile(project);
    const auto path=projectReferenceViewsPath(project);std::filesystem::create_directories(path.parent_path());static std::atomic<std::uint64_t> sequence{0};
    auto temporary=path;temporary+=".tmp."+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"."+std::to_string(sequence++);
    try{
        {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);output<<data;output.flush();if(!output)throw std::runtime_error("Cannot save complete project reference views");}
        std::filesystem::rename(temporary,path);
    }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}
}
