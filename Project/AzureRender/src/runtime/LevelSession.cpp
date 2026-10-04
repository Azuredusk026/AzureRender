#include "runtime/LevelSession.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "assets/GltfLoader.hpp"
#include <set>
namespace azurerender {
namespace {
void assignLegacyResourceKey(Level& level,const AssetDatabase& assets){
    if(!level.resourceKey.empty())return;
    for(const auto& resource:level.scene.resources){
        level.resourceKey+=resource.path.lexically_normal().generic_string()+":";
        for(const auto& record:assets.records())if(record.second.path==resource.path)level.resourceKey+=std::to_string(record.second.fingerprint);
    }
}
}

LevelSession::LevelSession(Project project, RuntimeLifecycle& runtime)
    : project_(std::move(project)), runtime_(runtime), assets_(project_) {
    assets_.refresh(); commit(load(project_.startupScene), project_.startupScene);
    nextPoll_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
}
LevelSession::~LevelSession(){if(cancelled_)*cancelled_=true;if(work_.valid())work_.wait();}
void LevelSession::request(std::string reference){
    ++generation_;if(cancelled_)*cancelled_=true;pending_=std::move(reference);preload_.reset();preparing_.reset();error_.clear();
}
void LevelSession::preload(const std::string& reference){
    if(preloaded(reference))return;
    prepared_.erase(reference);
    ++generation_;if(cancelled_)*cancelled_=true;preparing_.reset();preload_=reference;error_.clear();
}
void LevelSession::cancelPending(){++generation_;if(cancelled_)*cancelled_=true;pending_.reset();preload_.reset();preparing_.reset();}
void LevelSession::startWork(const std::string& reference,bool refresh){
    auto database=assets_;const auto project=project_;const auto generation=generation_;
    cancelled_=std::make_shared<std::atomic<bool>>(false);auto cancelled=cancelled_;const auto activeKey=level_.resourceKey;const auto meshes=level_.preparedMeshes;
    work_=std::async(std::launch::async,[database=std::move(database),project,reference,generation,refresh,cancelled,activeKey,meshes]()mutable{
        auto check=[&]{if(*cancelled)throw std::runtime_error("Level preparation cancelled");};
        check();database.refresh(false,check);check();
        const auto path=database.resolveReference(reference);Level level;
        if(path.extension()==".azurelevel")level=Level::load(path,database);
        else{auto legacy=project;legacy.startupScene=reference;level.scene=legacy.loadStartupScene();}
        assignLegacyResourceKey(level,database);
        if(level.resourceKey==activeKey)level.preparedMeshes=meshes;
        for(const auto& resource:level.scene.resources){
            if((!refresh || level.resourceKey!=activeKey) && !level.preparedMeshes.count(resource.path.lexically_normal().generic_string())){check();level.preparedMeshes.emplace(resource.path.lexically_normal().generic_string(),std::make_shared<const LoadedAsset>(loadGltfAsset(resource.path.string())));}
        }
        check();return Candidate{std::move(database),std::move(level),reference,generation,refresh};
    });
}
bool LevelSession::accept(Candidate candidate){
    // The renderer prepares against the candidate without publishing its database.
    commit(std::move(candidate.level),candidate.reference);assets_=std::move(candidate.assets);error_.clear();return true;
}
Level LevelSession::load(const std::string& reference) const {
    const auto path = assets_.resolveReference(reference);
    if (path.extension() == ".azurelevel") return Level::load(path, assets_);
    Project legacy = project_; legacy.startupScene = reference;
    Level level; level.scene = legacy.loadStartupScene();assignLegacyResourceKey(level,assets_); return level;
}
void LevelSession::commit(Level candidate, const std::string& reference) {
    const auto start=std::chrono::steady_clock::now();
    runtime_.replaceScene(candidate.scene, [&](auto& runtime) {
        for (const auto& node : candidate.components) installComponents(runtime.world(), runtime.entity(node.first), node.second);
        if (prepare_) prepare_(candidate);
    });
    level_ = std::move(candidate); reference_ = reference; ++revision_;
    commitMilliseconds_=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
bool LevelSession::poll() {
    try {
        if(runtime_.state()==RuntimeLifecycle::State::Stopped){cancelPending();return false;}
        if(work_.valid()){
            if(work_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
                try{
                    auto candidate=work_.get();
                    if(candidate.generation==generation_){
                        if(candidate.refresh){
                            if(candidate.level.reloadKey()!=level_.reloadKey()){prepared_.clear();preparing_=std::move(candidate);}
                            else{assets_=std::move(candidate.assets);error_.clear();}
                        }else{
                            preparing_=std::move(candidate);
                        }
                    }
                }catch(const std::exception&){if(cancelled_ && !*cancelled_)throw;}
                nextPoll_=std::chrono::steady_clock::now()+std::chrono::milliseconds(500);
            }
        }
        if(preparing_){
            if(!upload_ || upload_(preparing_->level)){
                if(preparing_->refresh){auto candidate=std::move(*preparing_);preparing_.reset();return accept(std::move(candidate));}
                if(prepared_.size()>=2)prepared_.erase(prepared_.begin());
                const auto reference=preparing_->reference;
                prepared_.insert_or_assign(reference,std::move(*preparing_));preparing_.reset();
            }else return false;
        }
        if(pending_){
            const auto reference=*pending_;
            if(reference==reference_){pending_.reset();commit(level_,reference);error_.clear();return true;}
            if(auto found=prepared_.find(reference);found!=prepared_.end() && preloaded(reference)){
                auto candidate=found->second;pending_.reset();return accept(std::move(candidate));
            }
            prepared_.erase(reference);
            if(!work_.valid())startWork(reference,false);
        }else if(preload_){if(!work_.valid()){const auto reference=*preload_;preload_.reset();startWork(reference,false);}}
        else if(!work_.valid() && std::chrono::steady_clock::now()>=nextPoll_){startWork(reference_,true);}
    } catch (const std::exception& error) {
        pending_.reset();
        preload_.reset();preparing_.reset();
        if (error_ != error.what()) RuntimeDiagnostics::instance().warning("runtime", error.what());
        error_ = error.what();
    }
    return false;
}
} // namespace azurerender
