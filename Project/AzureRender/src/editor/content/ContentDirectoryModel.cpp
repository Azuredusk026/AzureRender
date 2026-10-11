#include "ContentDirectoryModel.hpp"
#include "AssetCatalog.hpp"
#include <algorithm>
#include <cctype>
namespace azurerender {
namespace {
std::string normalized(const std::string& path){
    const auto value=std::filesystem::u8path(path);
    if(value.is_absolute()||value.has_root_name()||path.find(':')!=std::string::npos||path.find('\\')!=std::string::npos)throw std::invalid_argument("Directory requires a relative virtual path");
    for(const auto& part:value)if(part=="..")throw std::invalid_argument("Directory escapes its mount");
    const auto result=value.lexically_normal().generic_u8string();return result=="."?std::string():result;
}
std::string lower(std::string value){for(auto& c:value)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;}
std::string signature(const EditorContext& context){auto value=nlohmann::json::array();for(const auto& resource:context.scene().resources)value.push_back({resource.id,resource.path.generic_u8string()});for(const auto& node:context.scene().nodes)value.push_back(node.resourceId);return value.dump();}
}
void ContentDirectoryModel::cancelRefresh(){if(pending_.valid()){cancelled_->store(true);try{pending_.get();}catch(const std::exception&){}cancelled_.reset();}nextPoll_=std::chrono::steady_clock::now()+std::chrono::milliseconds(300);}
void ContentDirectoryModel::retireCatalog(){
    if(catalog_.empty()&&snapshots_.empty())return;
    // One owned reclamation job bounds retained data. It contains no document
    // pointer, and the model joins it before destruction or another retirement.
    if(reclamation_.valid())reclamation_.get();
    reclamation_=std::async(std::launch::async,[catalog=std::move(catalog_),snapshots=std::move(snapshots_)]() mutable {
        // Async shared states may retain the callable until future destruction.
        // Move its payload to locals so the worker releases the final owners.
        const auto retired=std::make_pair(std::move(catalog),std::move(snapshots));
        static_cast<void>(retired);
    });
    catalog_=nlohmann::json::array();snapshots_.clear();
}
void ContentDirectoryModel::reset(){cancelRefresh();retireCatalog();owner_=nullptr;revision_=0;databaseOwner_=nullptr;databaseVersion_=0;sceneSignature_.clear();stamps_.clear();directories_=nlohmann::json::array();diagnostics_=nlohmann::json::array();}
void ContentDirectoryModel::rebuild(EditorContext& context,bool enumerate){
    auto catalog=assetCatalog(context);retireCatalog();catalog_=std::move(catalog);++catalogBuilds_;owner_=&context;revision_=context.revision();++version_;
    sceneSignature_=signature(context);databaseOwner_=context.isProject()?&context.assets():nullptr;databaseVersion_=databaseOwner_?databaseOwner_->version():0;
    if(!enumerate)return;
    directories_=nlohmann::json::array();diagnostics_=nlohmann::json::array();stamps_.clear();
    if(!context.isProject())return;
    for(const auto& [mount,root]:context.project().mounts){
        std::error_code error;
        if(!std::filesystem::is_directory(root,error)){diagnostics_.push_back({{"mount",mount},{"message","Mount directory is unavailable"}});continue;}
        const auto append=[&](const std::filesystem::path& directory){
            std::error_code ec;stamps_[directory]=std::filesystem::last_write_time(directory,ec);
            auto path=directory.lexically_relative(root).generic_u8string();if(path==".")path.clear();
            directories_.push_back({{"mount",mount},{"path",path},{"reference",mount+":/"+path}});
        };append(root);++directoryEnumerations_;
        std::filesystem::recursive_directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,error),end;
        while(it!=end){
            if(error){diagnostics_.push_back({{"mount",mount},{"message",error.message()}});break;}
            if(it->is_symlink(error))it.disable_recursion_pending();
            else if(it->is_directory(error)){if(it->path().filename()==".azure"||it->path().filename()==".git")it.disable_recursion_pending();else append(it->path());}
            it.increment(error);
        }
    }
    nextPoll_=std::chrono::steady_clock::now()+std::chrono::milliseconds(300);
}
void ContentDirectoryModel::refresh(EditorContext& context){
    cancelRefresh();
    if(context.isProject()){context.assets().refresh();context.notifyAssetVersionChanged();}rebuild(context);
}
void ContentDirectoryModel::poll(EditorContext& context){
    if(owner_!=&context){reset();rebuild(context);}
    if(pending_.valid()){
        if(pending_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
        try{auto result=pending_.get();if(context.revision()==pendingRevision_){context.assets()=std::move(result);context.notifyAssetVersionChanged();}rebuild(context);}
        catch(const std::exception& error){diagnostics_.push_back({{"message",error.what()}});snapshots_.clear();}
    }
    const auto* database=context.isProject()?&context.assets():nullptr;
    if(databaseOwner_!=database||(database&&databaseVersion_!=database->version()))rebuild(context);
    else if(revision_!=context.revision()){const auto current=signature(context);if(current!=sceneSignature_)rebuild(context,false);else revision_=context.revision();}
    const auto now=std::chrono::steady_clock::now();if(now<nextPoll_||!context.isProject()||context.importing())return;
    nextPoll_=now+std::chrono::milliseconds(300);
    bool changed=false;for(const auto& [path,time]:stamps_){std::error_code error;if(std::filesystem::last_write_time(path,error)!=time||error){changed=true;break;}}
    if(changed){
        auto database=context.assets();pendingRevision_=context.revision();cancelled_=std::make_shared<std::atomic_bool>(false);
        pending_=std::async(std::launch::async,[database=std::move(database),cancelled=cancelled_]() mutable {
            database.refresh(false,[cancelled]{if(cancelled->load())throw std::runtime_error("Directory scan cancelled");});return database;
        });
    }
}
std::shared_ptr<const ContentDirectorySnapshot> ContentDirectoryModel::snapshot(EditorContext& context,const std::string& mount,const std::string& requested,const std::string& type,const std::string& search){
    const auto path=normalized(requested);
    if(!mount.empty()&&(!context.isProject()||!context.project().mounts.count(mount)))throw std::invalid_argument("Unknown content mount");
    if(mount.empty()&&!path.empty())throw std::invalid_argument("Directory requires a mount");
    poll(context);const auto key=nlohmann::json::array({mount,path,type,search}).dump();const auto found=snapshots_.find(key);if(found!=snapshots_.end())return found->second;
    auto result=std::make_shared<ContentDirectorySnapshot>();result->version=version_;result->mount=mount;result->path=path;result->diagnostics=diagnostics_;
    if(!mount.empty()){
        const auto directory=context.project().resolve(mount+":/"+path);std::error_code error;
        if(!std::filesystem::is_directory(directory,error))throw std::invalid_argument("Content directory is unavailable");
    }
    for(const auto& directory:directories_){
        const auto relative=directory.at("path").get<std::string>();
        if(mount.empty()){if(relative.empty())result->directories.push_back(directory);}
        else if(directory.at("mount")==mount&&!relative.empty()&&normalized(std::filesystem::u8path(relative).parent_path().generic_u8string())==path)result->directories.push_back(directory);
    }
    const auto filter=lower(search);
    for(const auto& asset:catalog_){
        if(!type.empty()&&asset.at("type")!=type)continue;const auto reference=asset.at("path").get<std::string>();
        if(!filter.empty()&&lower(reference).find(filter)==std::string::npos)continue;
        if(!mount.empty()){
            const auto prefix=mount+":/";if(reference.rfind(prefix,0)!=0)continue;
            const auto parent=normalized(std::filesystem::u8path(reference.substr(prefix.size())).parent_path().generic_u8string());
            if(parent!=path)continue;
        }result->assets.push_back(asset);
    }
    if(snapshots_.size()>=64)snapshots_.clear();snapshots_[key]=result;return result;
}
nlohmann::json ContentDirectoryModel::statistics() const{return {{"catalogBuilds",catalogBuilds_},{"directoryEnumerations",directoryEnumerations_},{"snapshots",snapshots_.size()},{"refreshing",pending_.valid()}};}
nlohmann::json ContentDirectoryModel::describe(const ContentDirectorySnapshot& snapshot) const{return {{"version",snapshot.version},{"mount",snapshot.mount},{"path",snapshot.path},{"assets",snapshot.assets},{"directories",snapshot.directories},{"diagnostics",snapshot.diagnostics},{"statistics",statistics()}};}
}
