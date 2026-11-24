#include "ShaderHotReloader.hpp"
#include "ShaderCompileJob.hpp"
#include <chrono>
#include <future>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <thread>
#include <algorithm>
namespace azurerender {
ShaderReloadOptions loadShaderReloadOptions(const std::filesystem::path& path){
    if(std::filesystem::file_size(path)>1024*1024)throw std::invalid_argument("Shader reload descriptor exceeds budget");
    std::ifstream input(path);nlohmann::json data;input>>data;
    if(data.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported shader reload descriptor");
    const auto resolve=[&](const std::string& value){auto p=std::filesystem::u8path(value);return std::filesystem::absolute(p.is_absolute()?p:path.parent_path()/p);};
    ShaderReloadOptions options;options.sourceRoot=resolve(data.at("sourceRoot").get<std::string>());
    options.binaryRoot=resolve(data.at("binaryRoot").get<std::string>());options.scratchRoot=resolve(data.at("scratchRoot").get<std::string>());
    options.compiler=resolve(data.at("compiler").get<std::string>());
    options.pollIntervalMs=data.value("pollIntervalMs",500u);options.timeoutMs=data.value("timeoutMs",60000u);
    options.sourceBytes=data.value("sourceBytes",static_cast<std::size_t>(16*1024*1024));
    for(const auto& program:data.at("programs"))options.programs.push_back({std::filesystem::u8path(program.at("source").get<std::string>()),
        std::filesystem::u8path(program.at("output").get<std::string>()),program.value("defines",std::vector<std::string>())});
    return options;
}
struct ShaderHotReloader::State {
    ShaderReloadOptions options;
    ShaderReloadStatus status;
    std::optional<ShaderReloadCandidate> candidate,active;
    std::future<ShaderCompileResult> job;
    std::atomic<bool> cancelled{false};
    std::uint64_t next=0;
    std::string observed;
    std::chrono::steady_clock::time_point lastPoll{};
    std::thread::id owner=std::this_thread::get_id();
    std::vector<std::filesystem::path> retired;
    void check() const{if(owner!=std::this_thread::get_id())throw std::logic_error("Shader reload requires its owner thread");}
    void discard(const ShaderReloadCandidate& value){
        const auto target=std::filesystem::absolute(value.directory.parent_path()).lexically_normal();
        const auto root=std::filesystem::absolute(options.scratchRoot).lexically_normal();
        if(target.parent_path()!=root)throw std::logic_error("Shader candidate retirement leaves scratch root");
        std::error_code error;std::filesystem::remove_all(target,error);
        if(error){retired.push_back(target);status.diagnostic="Shader retirement deferred: "+error.message();}
    }
    void reap(){
        retired.erase(std::remove_if(retired.begin(),retired.end(),[](const auto& path){
            std::error_code error;std::filesystem::remove_all(path,error);return !error;
        }),retired.end());
    }
};
ShaderHotReloader::ShaderHotReloader(ShaderReloadOptions options):state_(std::make_unique<State>()){
    state_->options=std::move(options);
    state_->options.sourceRoot=std::filesystem::absolute(state_->options.sourceRoot);
    state_->options.binaryRoot=std::filesystem::absolute(state_->options.binaryRoot);
    state_->options.scratchRoot=std::filesystem::absolute(state_->options.scratchRoot);
    // Scratch ownership is explicit; existing generation names are skipped.
    while(std::filesystem::exists(state_->options.scratchRoot/std::to_string(state_->next+1)))++state_->next;
    try{state_->observed=snapshotShaders(state_->options).fingerprint;}
    catch(const std::exception& error){state_->status.state=ShaderReloadState::Error;state_->status.diagnostic=error.what();}
}
ShaderHotReloader::~ShaderHotReloader(){
    state_->cancelled=true;
    try{if(state_->job.valid())state_->discard(state_->job.get().candidate);
        if(state_->candidate)state_->discard(*state_->candidate);
        if(state_->active)state_->discard(*state_->active);state_->reap();}catch(...){}
}
void ShaderHotReloader::requestRebuild(){
    auto& state=*state_;state.check();
    if(state.status.state==ShaderReloadState::Compiling || state.status.state==ShaderReloadState::Ready)return;
    try{
        state.reap();if(state.retired.size()>=4)throw std::runtime_error("Shader retirement budget awaits file release");
        const auto snapshot=snapshotShaders(state.options);state.observed=snapshot.fingerprint;
        state.cancelled=false;state.status.state=ShaderReloadState::Compiling;state.status.diagnostic.clear();
        const auto generation=++state.next;
        state.job=std::async(std::launch::async,[&state,snapshot,generation]{return compileShaderCandidate(state.options,snapshot,generation,state.cancelled);});
    }catch(const std::exception& error){state.status.state=ShaderReloadState::Error;state.status.diagnostic=error.what();}
}
void ShaderHotReloader::poll(){
    auto& state=*state_;state.check();
    if(state.status.state==ShaderReloadState::Compiling){
        if(state.job.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready)return;
        const auto result=state.job.get();state.status.diagnostic=result.diagnostic;
        if(state.cancelled || result.cancelled){state.status.state=ShaderReloadState::Cancelled;state.discard(result.candidate);}
        else if(result.passed){state.candidate=result.candidate;state.status.state=ShaderReloadState::Ready;}
        else{state.status.state=ShaderReloadState::Error;state.discard(result.candidate);}
        return;
    }
    if(state.status.state==ShaderReloadState::Ready)return;
    const auto now=std::chrono::steady_clock::now();
    if(now-state.lastPoll<std::chrono::milliseconds(state.options.pollIntervalMs))return;
    state.lastPoll=now;
    try{if(snapshotShaders(state.options).fingerprint!=state.observed)requestRebuild();}
    catch(const std::exception& error){state.status.state=ShaderReloadState::Error;state.status.diagnostic=error.what();}
}
ShaderReloadStatus ShaderHotReloader::status() const{state_->check();return state_->status;}
std::optional<ShaderReloadCandidate> ShaderHotReloader::candidate() const{state_->check();return state_->candidate;}
void ShaderHotReloader::finishCandidate(bool accepted,std::string diagnostic){
    auto& state=*state_;state.check();
    if(!state.candidate || state.status.state!=ShaderReloadState::Ready)throw std::logic_error("No complete shader candidate");
    state.status.diagnostic=std::move(diagnostic);
    if(accepted){if(state.active)state.discard(*state.active);state.active=std::move(state.candidate);state.status.activeGeneration=state.active->generation;}
    else state.discard(*state.candidate);
    state.candidate.reset();state.status.state=accepted?ShaderReloadState::Idle:ShaderReloadState::Error;
}
void ShaderHotReloader::cancel(){state_->check();state_->cancelled=true;
    if(state_->candidate){state_->discard(*state_->candidate);state_->candidate.reset();}
    if(state_->status.state!=ShaderReloadState::Compiling)state_->status.state=ShaderReloadState::Cancelled;
}
}
