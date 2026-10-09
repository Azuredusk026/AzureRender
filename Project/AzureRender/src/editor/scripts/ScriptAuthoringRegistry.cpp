#include "ScriptAuthoringRegistry.hpp"
#include <fstream>
#include <cctype>
namespace azurerender {
void ScriptAuthoringRegistry::add(Backend backend){
    if(backend.id.empty()||backend.label.empty()||backend.extension.empty()||backend.extension[0]!='.'||!backend.version||!backend.source)throw std::invalid_argument("Script template requires a registered backend descriptor");
    const auto id=backend.id;if(!backends_.emplace(id,std::move(backend)).second)throw std::invalid_argument("Duplicate script authoring backend");
}
nlohmann::json ScriptAuthoringRegistry::describe() const{auto result=nlohmann::json::array();for(const auto& [id,b]:backends_)result.push_back({{"id",id},{"label",b.label},{"extension",b.extension},{"version",b.version}});return result;}
nlohmann::json ScriptAuthoringRegistry::create(EditorContext& context,const std::string& id,const std::string& reference) const{
    const auto found=backends_.find(id);if(found==backends_.end())throw std::invalid_argument("Unknown script authoring backend");
    if(!context.isProject()||reference.rfind("engine:/",0)==0)throw std::invalid_argument("Script templates require a writable project mount");
    const auto path=context.project().resolve(reference);
    if(path.extension()!=found->second.extension)throw std::invalid_argument("Script extension does not match backend");
    if(std::filesystem::exists(path)||std::filesystem::exists(path.string()+".azmeta"))throw std::invalid_argument("Script destination already exists");
    const auto source=found->second.source(path.stem().u8string());
    if(source.empty()||source.size()>2097152)throw std::invalid_argument("Script template exceeds source budget");
    auto candidate=context.assets();std::filesystem::create_directories(path.parent_path());
    const auto temporary=path.u8string()+".template.tmp";
    if(std::filesystem::exists(temporary))throw std::invalid_argument("Script destination has an unfinished template write");
    bool published=false;
    try{
        {std::ofstream output(std::filesystem::u8path(temporary),std::ios::binary);output<<source;output.flush();if(!output)throw std::runtime_error("Cannot write script template");}
        std::filesystem::rename(std::filesystem::u8path(temporary),path);published=true;candidate.refresh();
        std::string asset;for(const auto& [uuid,record]:candidate.records())if(record.path.lexically_normal()==path.lexically_normal()){asset=uuid;break;}
        if(asset.empty())throw std::runtime_error("Created script requires an asset identity");context.assets()=std::move(candidate);
        return {{"asset",asset},{"path",reference},{"backend",id},{"templateVersion",found->second.version}};
    }catch(...){std::error_code ignored;std::filesystem::remove(std::filesystem::u8path(temporary),ignored);if(published){std::filesystem::remove(path,ignored);std::filesystem::remove(path.string()+".azmeta",ignored);}throw;}
}
ScriptAuthoringRegistry ScriptAuthoringRegistry::builtins(){
    ScriptAuthoringRegistry result;
    result.add({"lua","Lua",".lua",1,[](const std::string&){return std::string("-- Azure Lua behaviour template, version 1.\nfunction init()\nend\n\nfunction update(delta)\nend\n\nfunction trigger(other, entered)\nend\n\nfunction interact(actor)\nend\n\nfunction shutdown()\nend\n");}});
    const auto managed=[](const std::string& name){
        if(name.empty()||(!(std::isalpha(static_cast<unsigned char>(name[0]))||name[0]=='_'))||std::any_of(name.begin(),name.end(),[](unsigned char c){return c>127||(!std::isalnum(c)&&c!='_');}))throw std::invalid_argument("Managed template filename requires a valid ASCII class identifier");
        return "// Azure managed behaviour template, version 1.\nusing Azure.Engine;\nnamespace ProjectScripts;\npublic sealed class "+name+" : ScriptBehaviour\n{\n    public override void Init() { }\n    public override void Update(double delta) { }\n    public override void Trigger(string other, bool entered) { }\n    public override void Interact(string actor) { }\n    public override void Shutdown() { }\n}\n";
    };
    result.add({"coreclr","CoreCLR",".cs",1,managed});result.add({"nativeaot","NativeAOT",".cs",1,managed});return result;
}
}
