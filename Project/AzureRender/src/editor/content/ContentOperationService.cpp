#include "ContentOperationService.hpp"
#include "assets/TextureImportSettings.hpp"
#include "assets/GenerationManifest.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include "runtime/MaterialDocument.hpp"
#include <fstream>
#include <chrono>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace azurerender {
namespace {
using Json=nlohmann::json;namespace fs=std::filesystem;
std::string read(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary);if(!stream)throw std::runtime_error("Cannot read content: "+path.u8string());
    std::ostringstream result;result<<stream.rdbuf();if(stream.bad())throw std::runtime_error("Cannot read complete content");return result.str();
}
void write(const fs::path& path,const std::string& bytes) {
    fs::create_directories(path.parent_path());const auto temporary=fs::u8path(path.u8string()+".tmp");
    std::ofstream stream(temporary,std::ios::binary|std::ios::trunc);
    stream.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));stream.flush();if(!stream)throw std::runtime_error("Cannot publish content: "+path.u8string());
    stream.close();
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace content file: "+path.u8string());
#else
    fs::rename(temporary,path);
#endif
}
std::string uuid() {
    std::random_device random;std::ostringstream out;out<<std::hex<<std::setfill('0');
    for(int i=0;i<16;++i){if(i==4||i==6||i==8||i==10)out<<'-';unsigned value=random()&255;if(i==6)value=(value&15)|64;if(i==8)value=(value&63)|128;out<<std::setw(2)<<value;}return out.str();
}
bool jsonFile(const fs::path& path) {
    static const std::set<std::string> extensions{".gltf",".azurelevel",".azureprefab",".azscript",".json",".azmeta",".azureproject",".azurematerial"};return extensions.count(path.extension().u8string())!=0;
}
void rewrite(Json& value,const std::map<std::string,std::string>& map) {
    if(value.is_string()){const auto found=map.find(value.get<std::string>());if(found!=map.end())value=found->second;}
    else if(value.is_object()||value.is_array())for(auto& child:value)rewrite(child,map);
}
std::string rewriteScene(std::string content,const std::map<std::string,std::string>& mapping) {
    for(const auto& [from,to]:mapping) {
        std::ostringstream a,b;a<<std::quoted(from);b<<std::quoted(to);const auto source=a.str(),target=b.str();std::size_t at=0;
        while((at=content.find(source,at))!=std::string::npos){content.replace(at,source.size(),target);at+=target.size();}
    }return content;
}
std::string fingerprint(const std::string& value){return generationHash(value);}
bool writableFile(const fs::path& path) {
    if(!fs::exists(path))return true;
    return (fs::status(path).permissions()&fs::perms::owner_write)!=fs::perms::none;
}
}
ContentOperationService::ContentOperationService(Project project):project_(std::move(project)),database_(project_),journalRoot_(project_.file.parent_path()/".azure/content-transactions") {
    recover();database_.refresh();
}
std::filesystem::path ContentOperationService::writable(const std::string& reference) const {
    if(reference.rfind("engine:/",0)==0)throw std::invalid_argument("Content writes require a project mount");
    fs::path path;
    const auto mount=project_.mounts.find(reference.size()>=2&&reference.substr(reference.size()-2)==":/"?reference.substr(0,reference.size()-2):std::string());
    if(mount!=project_.mounts.end())path=mount->second;else path=project_.resolve(reference);
    const auto relative=path.lexically_relative(project_.file.parent_path());
    if(relative.empty()||*relative.begin()=="..")throw std::invalid_argument("Content path is outside this project");
    return path;
}
nlohmann::json ContentOperationService::version() {
    database_.refresh(true);auto result=Json::object();
    for(const auto& [id,record]:database_.records())result[id]={{"path",record.virtualPath},{"fingerprint",record.fingerprint}};
    result["project"]=fingerprint(read(project_.file));return result;
}
nlohmann::json ContentOperationService::dependencies(const std::string& asset,const std::string& direction) const {
    const auto id=asset.find(":/")==std::string::npos?asset:database_.idForPath(asset);
    const auto& record=database_.records().at(id);auto values=Json::array();
    if(direction=="forward") {
        for(const auto& dependency:record.dependencies)values.push_back(dependency);
    } else if(direction=="reverse") {
        for(const auto& [owner,candidate]:database_.records()) {
            if(std::find(candidate.dependencies.begin(),candidate.dependencies.end(),id)!=candidate.dependencies.end())values.push_back(owner);
        }
    } else {
        throw std::invalid_argument("Dependency direction must be forward or reverse");
    }
    return {{"asset",id},{"direction",direction},{"assets",values}};
}
nlohmann::json ContentOperationService::plan(const nlohmann::json& request) {
    if(plans_.size()>=64)throw std::runtime_error("Pending content plan budget exhausted");
    Plan value;value.version=version();const auto operation=request.at("operation").get<std::string>();const auto planId=uuid();
    std::map<std::string,std::string> relocated,identityMapping;
    value.description={{"id",planId},{"operation",operation},{"project",project_.id},{"affected",Json::array()},{"version",value.version}};
    const auto change=[&](const fs::path& path,std::optional<std::string> after){
        const auto relative=fs::weakly_canonical(path).lexically_relative(project_.file.parent_path());
        if(relative.empty()||*relative.begin()=="..")throw std::invalid_argument("Content file is outside this project");
        if(!writableFile(path)||!writableFile(path.parent_path()))throw std::runtime_error("Content is read-only: "+path.u8string());
        auto found=std::find_if(value.changes.begin(),value.changes.end(),[&](const auto& c){return c.path==path;});
        if(found!=value.changes.end()){found->after=std::move(after);return;}
        value.changes.push_back({path,fs::exists(path)?std::optional<std::string>(read(path)):std::nullopt,std::move(after)});
        value.description["affected"].push_back(path.lexically_relative(project_.file.parent_path()).generic_u8string());
    };
    if(operation=="material.write") {
        const auto path=writable(request.at("destination").get<std::string>());
        if(path.extension()!=".azurematerial"||!fs::is_directory(path.parent_path()))throw std::invalid_argument("Material destination requires an existing project directory and .azurematerial extension");
        if(request.value("create",false)&&fs::exists(path))throw std::invalid_argument("Material asset already exists");
        const auto document=MaterialDocument::parse(request.at("document"));
        for(const auto& texture:document.textures.items())if(!assetTypeRegistry().matches(database_.resolve(texture.value().at("asset").get<std::string>()),{"image"}))throw std::invalid_argument("Material texture requires an image asset");
        change(path,document.encode().dump(2)+"\n");
        if(!fs::exists(path)){const auto id=uuid();change(fs::u8path(path.u8string()+".azmeta"),Json{{"schemaVersion",1},{"id",id},{"settings",Json::object()}}.dump(2));value.description["asset"]=id;}
    }else if(operation=="content.create-directory") {
        const auto name=request.at("name").get<std::string>();if(name.empty()||name=="."||name==".."||name.find_first_of("/\\:")!=std::string::npos)throw std::invalid_argument("Invalid content directory name");
        const auto parent=writable(request.at("parent").get<std::string>());if(!fs::is_directory(parent)||!writableFile(parent))throw std::invalid_argument("Content parent is not writable");
        value.directory=parent/fs::u8path(name);if(fs::exists(*value.directory))throw std::invalid_argument("Content directory already exists");
        value.description["affected"].push_back(value.directory->lexically_relative(project_.file.parent_path()).generic_u8string());
    }else if(operation=="asset.delete") {
        if(request.value("referencePolicy",std::string("reject"))!="reject")throw std::invalid_argument("Deletion requires the reject reference policy");
        std::set<std::string> ids;
        for(const auto& asset:request.at("assets")){const auto text=asset.get<std::string>();ids.insert(text.find(":/")==std::string::npos?text:database_.idForPath(text));}
        if(ids.empty())throw std::invalid_argument("Deletion requires assets");
        for(const auto& id:ids) {
            const auto& record=database_.records().at(id);if(record.virtualPath==project_.startupScene)throw std::invalid_argument("Startup document is required by the project");
            const auto reverse=dependencies(id,"reverse");
            for(const auto& owner:reverse.at("assets"))if(!ids.count(owner.get<std::string>()))throw std::invalid_argument("Deletion would invalidate a referenced asset");
            change(writable(record.virtualPath),{});change(fs::u8path(record.path.u8string()+".azmeta"),{});
        }
    }else if(operation=="asset.reimport") {
        const auto reference=request.at("asset").get<std::string>();
        const auto id=reference.find(":/")==std::string::npos?reference:database_.idForPath(reference);
        const auto& record=database_.records().at(id);
        if(request.at("baseFingerprint").get<std::string>()!=std::to_string(record.fingerprint))throw std::invalid_argument("Reimport fingerprint is expired");
        const auto path=writable(record.virtualPath),sidecar=fs::u8path(path.u8string()+".azmeta");
        auto metadata=Json::parse(read(sidecar));const auto settings=request.at("settings");
        if(!settings.is_object())throw std::invalid_argument("Reimport settings require an object");
        metadata["settings"].update(settings);
        if(metadata["settings"].contains("texture")) {
            if(!assetTypeRegistry().matches(path,{"image"}))throw std::invalid_argument("Texture settings require image assets");
            static_cast<void>(TextureImportSettings::parse(metadata["settings"]["texture"]));
            metadata["settings"]["texture"]["schemaVersion"]=1;
        }
        change(sidecar,metadata.dump(2));value.description["asset"]=id;value.description["settings"]=metadata["settings"];
    }else if(operation=="texture.settings") {
        auto assets=request.value("assets",Json::array());
        if(request.contains("asset"))assets.push_back(request.at("asset"));
        if(assets.empty()||!request.at("values").is_object())throw std::invalid_argument("Texture settings require targets and named values");
        std::set<std::string> unique;
        for(const auto& input:assets) {
            const auto reference=input.get<std::string>();
            const auto id=reference.find(":/")==std::string::npos?reference:database_.idForPath(reference);
            if(!unique.insert(id).second)continue;
            const auto& record=database_.records().at(id);
            if(!assetTypeRegistry().matches(record.path,{"image"}))throw std::invalid_argument("Texture settings require image assets");
            const auto source=writable(record.virtualPath),sidecar=fs::u8path(source.u8string()+".azmeta");
            auto metadata=Json::parse(read(sidecar));
            auto current=metadata.value("settings",Json::object()).value("texture",Json::object());
            current.update(request.at("values"));
            static_cast<void>(TextureImportSettings::parse(current));current["schemaVersion"]=1;
            metadata["settings"]["texture"]=current;
            change(sidecar,metadata.dump(2));
        }
        value.description["assets"]=unique;
    }else if(operation=="asset.duplicate"&&request.value("includeDependencies",false)) {
        const auto input=request.at("asset").get<std::string>();
        const auto id=input.find(":/")==std::string::npos?input:database_.idForPath(input);
        const auto& source=database_.records().at(id);
        std::string destination=request.at("destination").get<std::string>();
        auto target=writable(destination);
        if(fs::is_directory(target)){target/=source.path.filename();destination+="/"+source.path.filename().u8string();}
        if(target.extension()!=source.path.extension())throw std::invalid_argument("Asset copies preserve the registered extension");
        std::set<std::string> closure;
        const auto collect=[&](const std::string& current,auto&& self)->void {
            if(!closure.insert(current).second)return;
            for(const auto& dependency:database_.records().at(current).dependencies)self(dependency,self);
        };
        collect(id,collect);
        std::map<std::string,std::string> mapping,locations;
        std::set<fs::path> targets;
        for(const auto& current:closure) {
            const auto& record=database_.records().at(current);
            writable(record.virtualPath);
            auto relative=record.path.lexically_relative(source.path.parent_path());
            if(relative.empty()||*relative.begin()=="..")relative=fs::path("_dependencies")/current/record.path.filename();
            const auto path=current==id?target:target.parent_path()/relative;
            if(!targets.insert(path).second||fs::exists(path)||fs::exists(fs::u8path(path.u8string()+".azmeta")))throw std::invalid_argument("Dependency copy target is unavailable");
            const auto slash=destination.rfind('/');
            const auto reference=current==id?destination:destination.substr(0,slash+1)+relative.generic_u8string();
            writable(reference);locations[current]=reference;mapping[current]=uuid();mapping[record.virtualPath]=reference;
        }
        relocated=locations;
        identityMapping=mapping;
        for(const auto& current:closure) {
            const auto& record=database_.records().at(current);const auto path=writable(locations.at(current));
            auto content=read(record.path);
            auto metadata=Json::parse(read(fs::u8path(record.path.u8string()+".azmeta")));
            rewrite(metadata,mapping);
            if(record.path.extension()==".azscene")content=rewriteScene(content,mapping);
            if(jsonFile(record.path)) {
                auto document=Json::parse(content);rewrite(document,mapping);
                if(record.path.extension()==".gltf")for(const auto* key:{"buffers","images"})for(auto& entry:document[key]) {
                    const auto uri=entry.value("uri",std::string());if(uri.empty()||uri.rfind("data:",0)==0)continue;
                    const auto dependency=fs::weakly_canonical(record.path.parent_path()/fs::u8path(uri));
                    for(const auto& copied:closure)if(database_.records().at(copied).path==dependency)
                        entry["uri"]=writable(locations.at(copied)).lexically_relative(path.parent_path()).generic_u8string();
                }
                content=document.dump(2);
            }
            change(path,content);change(fs::u8path(path.u8string()+".azmeta"),metadata.dump(2));
        }
        value.description["asset"]=mapping.at(id);value.description["identityMapping"]=mapping;value.description["destination"]=destination;
    }else {
        const auto input=request.at("asset").get<std::string>();const auto id=input.find(":/")==std::string::npos?input:database_.idForPath(input);
        const auto record=database_.records().at(id);const auto source=writable(record.virtualPath);const auto sidecar=fs::u8path(source.u8string()+".azmeta");
        {
            if(operation!="asset.move"&&operation!="asset.rename"&&operation!="asset.duplicate")throw std::invalid_argument("Unknown content operation");
            std::string destination;
            if(operation=="asset.rename") {
                const auto name=request.at("name").get<std::string>();if(name.empty()||name.find_first_of("/\\:")!=std::string::npos)throw std::invalid_argument("Invalid asset name");
                destination=record.virtualPath.substr(0,record.virtualPath.rfind('/')+1)+name;
            }else destination=request.at("destination").get<std::string>();
            auto target=writable(destination);if(fs::is_directory(target)){target/=source.filename();destination+="/"+source.filename().u8string();}
            if(!fs::is_directory(target.parent_path())||fs::exists(target)||fs::exists(fs::u8path(target.u8string()+".azmeta")))throw std::invalid_argument("Asset destination is unavailable");
            if(target.extension()!=source.extension())throw std::invalid_argument("Asset renames preserve the registered extension");
            auto content=read(source);auto metadata=Json::parse(read(sidecar));std::map<std::string,std::string> map{{record.virtualPath,destination}};
            if(operation=="asset.duplicate"){const auto duplicate=uuid();metadata["id"]=duplicate;map[id]=duplicate;value.description["asset"]=duplicate;}else value.description["asset"]=id;
            relocated[id]=destination;identityMapping=map;
            if(source.extension()==".azscene")content=rewriteScene(content,map);
            if(jsonFile(source)) {
                auto document=Json::parse(content);rewrite(document,map);
                if(source.extension()==".gltf")for(const auto* key:{"buffers","images"})for(auto& entry:document[key]) {
                    auto uri=entry.value("uri",std::string());if(uri.empty()||uri.rfind("data:",0)==0)continue;
                    const auto absolute=fs::weakly_canonical(source.parent_path()/fs::u8path(uri));entry["uri"]=absolute.lexically_relative(target.parent_path()).generic_u8string();
                }
                content=document.dump(2);
            }
            change(target,content);change(fs::u8path(target.u8string()+".azmeta"),metadata.dump(2));
            if(operation!="asset.duplicate") {
                change(source,{});change(sidecar,{});
                for(const auto& [owner,dependency]:database_.records()) {
                    if(owner==id)continue;
                    if(dependency.path.extension()==".azscene") {
                        const auto original=read(dependency.path),updated=rewriteScene(original,map);if(original!=updated)change(dependency.path,updated);continue;
                    }
                    if(!jsonFile(dependency.path))continue;
                    auto document=Json::parse(read(dependency.path));const auto original=document;rewrite(document,map);
                    if(dependency.path.extension()==".gltf")for(const auto* key:{"buffers","images"})for(auto& entry:document[key]) {
                        const auto uri=entry.value("uri",std::string());if(uri.empty()||uri.rfind("data:",0)==0)continue;
                        if(fs::weakly_canonical(dependency.path.parent_path()/fs::u8path(uri))==source)entry["uri"]=target.lexically_relative(dependency.path.parent_path()).generic_u8string();
                    }
                    if(document!=original)change(dependency.path,document.dump(2));
                }
                auto document=Json::parse(read(project_.file));const auto original=document;rewrite(document,map);if(document!=original)change(project_.file,document.dump(2));
            }
            value.description["destination"]=destination;
        }
    }
    if(!relocated.empty()) {
        const auto plannedBytes=[&](const fs::path& path) {
            const auto found=std::find_if(value.changes.begin(),value.changes.end(),[&](const auto& item){return item.path==path;});
            if(found==value.changes.end())return read(path);
            if(!found->after)throw std::invalid_argument("Provenance references removed content");
            return *found->after;
        };
        const auto referenceFor=[&](const fs::path& path) {
            for(const auto& [id,record]:database_.records())if(record.path==fs::weakly_canonical(path))
                return relocated.count(id)?relocated.at(id):record.virtualPath;
            throw std::invalid_argument("Provenance output is not a registered asset");
        };
        for(const auto& [id,record]:database_.records()) {
            if(operation=="asset.duplicate"&&!relocated.count(id))continue;
            const auto path=relocated.count(id)?writable(relocated.at(id)):record.path;
            const auto sidecar=fs::u8path(path.u8string()+".azmeta");
            auto metadata=Json::parse(plannedBytes(sidecar));const auto original=metadata;
            rewrite(metadata,identityMapping);
            for(const auto* field:{"generation","importGeneration"})if(metadata.contains(field)) {
                const auto previous=GenerationManifest::fromJson(metadata.at(field));auto manifest=previous;
                const auto transform=[&](const auto& hashes,bool imported) {
                    std::map<std::string,std::string> result;
                    for(const auto& [name,digest]:hashes) {
                        std::string reference;
                        if(imported&&name.find(":/")==std::string::npos)
                            reference=referenceFor(record.path.parent_path()/fs::u8path(name));
                        else {
                            const auto found=identityMapping.find(name);reference=found==identityMapping.end()?name:found->second;
                        }
                        fs::path resolved;
                        if(reference.find(":/")!=std::string::npos)resolved=project_.resolve(reference);
                        else {
                            const auto owner=database_.records().find(reference);
                            if(owner!=database_.records().end())resolved=owner->second.path;
                            else for(const auto& [oldId,newReference]:relocated)if(identityMapping.count(oldId)&&identityMapping.at(oldId)==reference)resolved=writable(newReference);
                        }
                        if(resolved.empty())throw std::invalid_argument("Unknown provenance identity");
                        result[reference]=generationHash(plannedBytes(resolved));
                    }
                    return result;
                };
                const bool imported=std::string(field)=="importGeneration";
                manifest.inputHashes=transform(previous.inputHashes,imported);
                manifest.dependencyHashes=transform(previous.dependencyHashes,imported);
                manifest.outputHashes=transform(previous.outputHashes,imported);
                metadata[field]=manifest.toJson();
                if(metadata[field]!=previous.toJson())metadata["provenanceTransform"]={{"operation",operation},{"source",previous.toJson()}};
            }
            if(metadata!=original)change(sidecar,metadata.dump(2));
        }
    }
    const auto description=value.description;plans_.emplace(planId,std::move(value));return description;
}
nlohmann::json ContentOperationService::apply(const std::string& planId,std::function<void()> check) {
    const auto found=plans_.find(planId);if(found==plans_.end())throw std::invalid_argument("Unknown content plan");
    auto plan=std::move(found->second);plans_.erase(found);
    if(version()!=plan.version)throw std::invalid_argument("Content plan is expired");
    for(const auto& change:plan.changes) {
        if(change.before.has_value()!=fs::exists(change.path)||(change.before&&read(change.path)!=*change.before))throw std::invalid_argument("Content changed after planning");
        if(!writableFile(change.path))throw std::invalid_argument("Content became read-only");
    }
    if(plan.directory&&fs::exists(*plan.directory))throw std::invalid_argument("Directory appeared after planning");
    const auto folder=journalRoot_/planId;fs::create_directories(folder);auto journal=plan.description;journal["state"]="prepared";journal["files"]=Json::array();
    journal["sequence"]=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    journal["invalidatedRedo"]=redo_;
    if(plan.directory)journal["directory"]=plan.directory->lexically_relative(project_.file.parent_path()).generic_u8string();
    for(std::size_t index=0;index<plan.changes.size();++index) {
        const auto& change=plan.changes[index];const auto name=std::to_string(index);
        if(change.before)write(folder/(name+".before"),*change.before);if(change.after)write(folder/(name+".after"),*change.after);
        journal["files"].push_back({{"path",change.path.lexically_relative(project_.file.parent_path()).generic_u8string()},{"before",change.before.has_value()},{"after",change.after.has_value()},
            {"beforeHash",change.before?fingerprint(*change.before):""},{"afterHash",change.after?fingerprint(*change.after):""}});
    }
    write(folder/"journal.json",journal.dump(2));
    try {
        if(check)check();journal["state"]="applying";write(folder/"journal.json",journal.dump(2));
        if(plan.directory)fs::create_directory(*plan.directory);
        for(const auto& change:plan.changes){if(change.after)write(change.path,*change.after);else fs::remove(change.path);if(check)check();}
        auto project=Project::load(project_.file);AssetDatabase validation(project);validation.refresh(true);
        journal["state"]="committed";write(folder/"journal.json",journal.dump(2));
        redo_.clear();
        project_=std::move(project);database_=std::move(validation);history_.push_back(planId);return journal;
    }catch(...) {
        const auto failure=std::current_exception();restore(folder,true);database_=AssetDatabase(Project::load(project_.file));database_.refresh(true);std::rethrow_exception(failure);
    }
}
void ContentOperationService::restore(const std::filesystem::path& folder,bool before) {
    restoreJournal(folder,project_.file.parent_path(),project_.id,before);
}
void ContentOperationService::restoreJournal(const fs::path& folder,const fs::path& root,const std::string& projectId,bool before) {
    auto journal=Json::parse(read(folder/"journal.json"));std::size_t index=0;
    if(journal.at("project")!=projectId)throw std::invalid_argument("Content journal belongs to another project");
    for(const auto& file:journal.at("files")) {
        const auto relative=fs::u8path(file.at("path").get<std::string>());
        const auto confined=fs::weakly_canonical(root/relative).lexically_relative(root);
        if(relative.is_absolute()||confined.empty()||*confined.begin()=="..")throw std::invalid_argument("Invalid content journal path");
        if(file.at(before?"before":"after").get<bool>()) {
            const auto backup=read(folder/(std::to_string(index)+(before?".before":".after")));
            if(fingerprint(backup)!=file.at(before?"beforeHash":"afterHash"))throw std::invalid_argument("Content journal backup is damaged");
        }++index;
    }
    index=0;
    if(!before&&journal.contains("directory")) {
        const auto path=fs::weakly_canonical(root/fs::u8path(journal.at("directory").get<std::string>()));
        const auto relative=path.lexically_relative(root);if(relative.empty()||*relative.begin()=="..")throw std::invalid_argument("Invalid journal directory");
        fs::create_directory(path);
    }
    for(const auto& file:journal.at("files")) {
        const auto relative=fs::u8path(file.at("path").get<std::string>());const auto path=fs::weakly_canonical(root/relative);
        const auto confined=path.lexically_relative(root);if(relative.is_absolute()||confined.empty()||*confined.begin()=="..")throw std::invalid_argument("Invalid content journal path");
        if(file.at(before?"before":"after").get<bool>())write(path,read(folder/(std::to_string(index)+(before?".before":".after"))));else fs::remove(path);++index;
    }
    if(before&&journal.contains("directory")) {
        const auto path=fs::weakly_canonical(root/fs::u8path(journal.at("directory").get<std::string>()));
        const auto relative=path.lexically_relative(root);if(relative.empty()||*relative.begin()=="..")throw std::invalid_argument("Invalid journal directory");
        if(fs::is_directory(path)&&fs::is_empty(path))fs::remove(path);
    }
    journal["state"]=before?"rolled-back":"committed";write(folder/"journal.json",journal.dump(2));
}
void ContentOperationService::undo() {
    if(history_.empty())throw std::invalid_argument("No content transaction to undo");
    const auto folder=journalRoot_/history_.back();auto journal=Json::parse(read(folder/"journal.json"));
    for(const auto& file:journal.at("files")) {
        const auto path=project_.file.parent_path()/fs::u8path(file.at("path").get<std::string>());
        if(fs::exists(path)!=file.at("after").get<bool>()||(fs::exists(path)&&fingerprint(read(path))!=file.at("afterHash")))throw std::invalid_argument("Content undo would overwrite an external modification");
    }
    if(journal.contains("directory")) {
        const auto directory=project_.file.parent_path()/fs::u8path(journal.at("directory").get<std::string>());
        if(fs::exists(directory)&&!fs::is_empty(directory))throw std::invalid_argument("Directory undo would remove external content");
    }
    journal["state"]="undoing";write(folder/"journal.json",journal.dump(2));
    try {
        restore(folder,true);auto project=Project::load(project_.file);AssetDatabase validated(project);validated.refresh(true);
        journal["state"]="undone";write(folder/"journal.json",journal.dump(2));
        redo_.push_back(history_.back());history_.pop_back();project_=std::move(project);database_=std::move(validated);
    } catch(...) {
        const auto failure=std::current_exception();restore(folder,false);std::rethrow_exception(failure);
    }
}
void ContentOperationService::redo() {
    if(redo_.empty())throw std::invalid_argument("No content transaction to redo");
    const auto folder=journalRoot_/redo_.back();auto journal=Json::parse(read(folder/"journal.json"));
    for(const auto& file:journal.at("files")) {
        const auto path=project_.file.parent_path()/fs::u8path(file.at("path").get<std::string>());
        if(fs::exists(path)!=file.at("before").get<bool>()||(fs::exists(path)&&fingerprint(read(path))!=file.at("beforeHash")))throw std::invalid_argument("Content redo would overwrite an external modification");
    }
    if(journal.contains("directory")&&fs::exists(project_.file.parent_path()/fs::u8path(journal.at("directory").get<std::string>())))throw std::invalid_argument("Content redo destination is unavailable");
    journal["state"]="redoing";write(folder/"journal.json",journal.dump(2));
    try {
        restore(folder,false);auto project=Project::load(project_.file);AssetDatabase validated(project);validated.refresh(true);
        history_.push_back(redo_.back());redo_.pop_back();project_=std::move(project);database_=std::move(validated);
    } catch(...) {
        const auto failure=std::current_exception();restore(folder,true);journal["state"]="undone";write(folder/"journal.json",journal.dump(2));std::rethrow_exception(failure);
    }
}
void ContentOperationService::recoverProject(const fs::path& projectFile) {
    const auto file=fs::weakly_canonical(projectFile),root=file.parent_path();
    const auto journals=root/".azure/content-transactions";
    if(!fs::exists(journals))return;
    const auto descriptor=Json::parse(read(file));const auto id=descriptor.at("id").get<std::string>();
    for(const auto& entry:fs::directory_iterator(journals))if(entry.is_directory()&&fs::exists(entry.path()/"journal.json")) {
        const auto journal=Json::parse(read(entry.path()/"journal.json"));
        if(journal.at("project")!=id)throw std::invalid_argument("Content journal belongs to another project");
        const auto state=journal.at("state").get<std::string>();
        if(state=="prepared"||state=="applying"||state=="undoing") {
            restoreJournal(entry.path(),root,id,true);
            if(state=="undoing"){auto undone=journal;undone["state"]="undone";write(entry.path()/"journal.json",undone.dump(2));}
        }else if(state=="redoing")restoreJournal(entry.path(),root,id,false);
    }
}
void ContentOperationService::recover() {
    recoverProject(project_.file);
    if(!fs::exists(journalRoot_))return;
    std::map<std::int64_t,std::vector<std::string>> committed,undone;std::set<std::string> invalidated;
    for(const auto& entry:fs::directory_iterator(journalRoot_))if(entry.is_directory()&&fs::exists(entry.path()/"journal.json")) {
        const auto journal=Json::parse(read(entry.path()/"journal.json"));
        if(journal.at("project")!=project_.id)throw std::invalid_argument("Content journal belongs to another project");
        const auto state=journal.at("state").get<std::string>();if(state=="applying"||state=="prepared")restore(entry.path(),true);
        if(state=="committed"||state=="undone")for(const auto& id:journal.value("invalidatedRedo",Json::array()))invalidated.insert(id.get<std::string>());
        if(state=="committed")committed[journal.value("sequence",std::int64_t(0))].push_back(entry.path().filename().u8string());
        if(state=="undone")undone[journal.value("sequence",std::int64_t(0))].push_back(entry.path().filename().u8string());
    }
    history_.clear();for(const auto& [sequence,ids]:committed){static_cast<void>(sequence);history_.insert(history_.end(),ids.begin(),ids.end());}
    const auto latest=committed.empty()?std::int64_t(0):committed.rbegin()->first;
    redo_.clear();for(auto iterator=undone.rbegin();iterator!=undone.rend();++iterator)if(iterator->first>latest)
        for(const auto& id:iterator->second)if(!invalidated.count(id))redo_.push_back(id);
}
nlohmann::json ContentOperationService::textureSettings(const std::vector<std::string>& assets) const {
    if(assets.empty())throw std::invalid_argument("Texture inspection requires image assets");
    auto rows=Json::array(),values=Json::object(),mixed=Json::array();
    for(const auto& reference:assets) {
        const auto id=reference.find(":/")==std::string::npos?reference:database_.idForPath(reference);
        const auto& record=database_.records().at(id);
        if(!assetTypeRegistry().matches(record.path,{"image"}))throw std::invalid_argument("Texture inspection requires image assets");
        const auto metadata=Json::parse(read(fs::u8path(record.path.u8string()+".azmeta")));
        const auto settings=TextureImportSettings::parse(metadata.value("settings",Json::object()).value("texture",Json::object())).describe();
        rows.push_back({{"id",id},{"path",record.virtualPath},{"fingerprint",std::to_string(record.fingerprint)},{"settings",settings}});
        if(rows.size()==1)values=settings;
        else for(const auto& item:settings.items())if(values.at(item.key())!=item.value()&&std::find(mixed.begin(),mixed.end(),Json(item.key()))==mixed.end())mixed.push_back(item.key());
    }
    return {{"assets",rows},{"values",values},{"mixed",mixed}};
}
nlohmann::json ContentOperationService::inspect(const std::string& asset) const {
    const auto id=asset.find(":/")==std::string::npos?asset:database_.idForPath(asset);
    const auto& record=database_.records().at(id);const auto* type=assetTypeRegistry().classify(record.path);
    const bool ready=fs::is_regular_file(record.path);
    auto result=Json{{"id",id},{"path",record.virtualPath},{"source",record.path.u8string()},{"type",type?type->label:"Unknown"},
        {"ready",ready},{"fingerprint",std::to_string(record.fingerprint)},{"dependencies",Json::array()},{"referencedBy",Json::array()}};
    for(const auto& dependency:record.dependencies)result["dependencies"].push_back({{"id",dependency},{"path",database_.records().at(dependency).virtualPath}});
    const auto reverse=dependencies(id,"reverse");
    for(const auto& owner:reverse.at("assets"))result["referencedBy"].push_back({{"id",owner},{"path",database_.records().at(owner.get<std::string>()).virtualPath}});
    if(record.generation)result["generation"]=record.generation->toJson();
    if(record.importGeneration)result["importGeneration"]=record.importGeneration->toJson();
    const auto sidecar=fs::u8path(record.path.u8string()+".azmeta");
    if(fs::is_regular_file(sidecar))result["settings"]=Json::parse(read(sidecar)).value("settings",Json::object());
    try{result["sourceChanged"]=ready&&fingerprint(read(record.path))!=fingerprint(database_.readSource(id));}
    catch(const std::exception& error){result["diagnostic"]=error.what();}
    return result;
}
}
