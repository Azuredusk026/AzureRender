#include "scripting/dotnet/ManagedScriptRuntime.hpp"
#include "scripting/dotnet/ManagedModule.hpp"
#include "scripting/ScriptHostSession.hpp"
#include "scripting/GeneratedBindings.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>
namespace azurerender {
namespace {
using Json=nlohmann::json;
std::string read(const std::filesystem::path& path,std::size_t limit) {
    if(std::filesystem::file_size(path)>limit)throw std::length_error("Managed script content exceeds byte budget");
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot read managed script content");
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
void configuration(const Json& config) {
    const auto backend=config.at("backend").get<std::string>();
    if(backend!="coreclr" && backend!="nativeaot")throw std::invalid_argument("Unknown managed backend");
    for(const auto& field:config.items())if(field.key()!="schemaVersion" && field.key()!="backend" && field.key()!="module"
        && !(backend=="coreclr" && field.key()=="runtimeConfig"))throw std::invalid_argument("Unknown managed configuration field");
    for(const auto* field:{"module","runtimeConfig"}) {
        if(backend=="nativeaot" && std::string(field)=="runtimeConfig")continue;
        if(!config.at(field).is_string() || config.at(field).get<std::string>().empty())throw std::invalid_argument("Managed module requires a resource reference");
    }
}
}
struct ManagedScriptRuntime::Impl {
    RuntimeLifecycle& runtime;
    AssetDatabase& assets;
    std::string backend;
    std::shared_ptr<ScriptBindingHost> host;
    ScriptHostSession session;
    ManagedModule module;
    struct Entry {ScriptObject object;std::string asset,attempted;bool active=false;};
    std::map<ecs::Entity,Entry> entries;
    std::vector<std::string> errors;
    bool closed=false;
    std::uint64_t revision=0;
    std::chrono::steady_clock::time_point nextPoll{};
    Impl(RuntimeLifecycle& r,GameRuntime& g,AssetDatabase& a,const Json& config)
        :runtime(r),assets(a),backend(config.at("backend").get<std::string>()),host(std::make_shared<ScriptBindingHost>(r,g)),session(host),
         module(backend,a.resolveReference(config.at("module").get<std::string>()),
             backend=="coreclr"?a.resolveReference(config.at("runtimeConfig").get<std::string>()):std::filesystem::path()) {
        const auto handshake=module.call(session.api(),{{"operation","handshake"},{"apiVersion",kScriptBindingVersion}});
        if(handshake.at("bindingHash")!=kScriptBindingHash)throw std::runtime_error("Managed module binding description is stale");
        for(const auto& asset:assets.records())if(asset.second.path.extension()==".azscript")
            static_cast<void>(source(asset.second.virtualPath));
    }
    void open() const {if(closed)throw std::logic_error("Managed script runtime is closed");}
    void error(const std::string& text) {
        if(errors.empty() || errors.back()!=text){if(errors.size()==128)errors.erase(errors.begin());errors.push_back(text);RuntimeDiagnostics::instance().warning("script",text);}
    }
    Json command(const Json& request){return module.call(session.api(),request);}
    struct Source {Json manifest;std::string fingerprint;std::filesystem::path assembly;};
    Source source(const std::string& reference) {
        const auto path=assets.resolveReference(reference);
        if(path.extension()!=".azscript")throw std::invalid_argument("Managed script requires a versioned .azscript manifest");
        const auto bytes=read(path,kScriptInteropByteLimit);auto manifest=Json::parse(bytes);
        if(!manifest.is_object() || !manifest.at("schemaVersion").is_number_integer() || manifest.at("schemaVersion")!=1
            || !manifest.at("type").is_string() || manifest.at("type").get<std::string>().empty())throw std::invalid_argument("Invalid managed script manifest");
        for(const auto& field:manifest.items())if(field.key()!="schemaVersion" && field.key()!="type" && field.key()!="assembly")throw std::invalid_argument("Unknown managed script manifest field");
        Source value{manifest,bytes,{}};
        if(backend=="coreclr") {
            value.assembly=assets.resolveReference(manifest.at("assembly").get<std::string>());
            value.fingerprint+=read(value.assembly,16*1024*1024);
        }
        return value;
    }
    void callback(Entry& entry,const std::string& name,Json parameters=Json::object()) {
        if(!entry.active)return;
        host->beginCallback();
        try {
            parameters["operation"]="callback";parameters["id"]=entry.object.node;parameters["callback"]=name;
            command(parameters);host->endCallback(true);
        }catch(const std::exception& failure){host->endCallback(false);entry.active=false;error(entry.asset+": "+failure.what());}
    }
    void closeEntry(Entry& entry) {
        if(entry.active && host->valid(entry.object))callback(entry,"shutdown");
        command({{"operation","closeObject"},{"id",entry.object.node}});entry.active=false;
    }
    bool prepare(Entry& entry,const Source& content) {
        entry.attempted=content.fingerprint;host->beginCallback(true);bool callbackActive=true;
        try {
            command({{"operation","prepare"},{"id",entry.object.node},{"object",ScriptBindingHost::encode(entry.object)},
                {"assembly",content.assembly.u8string()},{"type",content.manifest.at("type")}});
            host->endCallback(true);callbackActive=false;
            host->commitInitialization([&]{if(entry.active)callback(entry,"shutdown");});
            command({{"operation","activate"},{"id",entry.object.node}});entry.active=true;return true;
        }catch(const std::exception& failure) {
            if(callbackActive)host->endCallback(false);else host->cancelInitialization();
            try{command({{"operation","cancel"},{"id",entry.object.node}});}catch(...){}
            error(entry.asset+": "+failure.what());return false;
        }
    }
    void synchronize() {
        open();
        if(revision!=runtime.sceneRevision()) {
            for(auto& entry:entries)command({{"operation","closeObject"},{"id",entry.second.object.node}});
            entries.clear();revision=runtime.sceneRevision();
        }
        for(auto it=entries.begin();it!=entries.end();) {
            const auto* script=runtime.world().tryGet<game::Script>(it->first);
            if(!script || !script->enabled || script->asset!=it->second.asset || !host->valid(it->second.object)) {closeEntry(it->second);it=entries.erase(it);}else ++it;
        }
        runtime.world().each<game::Script>([&](auto entity,const auto& script) {
            if(!script.enabled || script.asset.empty() || entries.count(entity))return;
            auto inserted=entries.emplace(entity,Entry{host->object(runtime.nodeId(entity)),script.asset,{},false});
            try{prepare(inserted.first->second,source(script.asset));}catch(const std::exception& failure){error(script.asset+": "+failure.what());}
        });
    }
    void reload() {
        synchronize();
        if(backend=="nativeaot"){error("NativeAOT reload requires a rebuilt module and a new process");return;}
        for(auto& entry:entries)try {
            const auto content=source(entry.second.asset);if(content.fingerprint!=entry.second.attempted)prepare(entry.second,content);
        }catch(const std::exception& failure){error(entry.second.asset+": "+failure.what());}
    }
    void shutdown() noexcept {
        if(closed)return;
        for(auto& entry:entries)try{closeEntry(entry.second);}catch(...){}
        entries.clear();try{command({{"operation","closeSession"}});}catch(...){}
        session.close();host->close();closed=true;
    }
};
ManagedScriptRuntime::ManagedScriptRuntime(RuntimeLifecycle& runtime,GameRuntime& game,AssetDatabase& assets,const Json& config) {
    configuration(config);impl_=std::make_unique<Impl>(runtime,game,assets,config);initialize();
}
ManagedScriptRuntime::~ManagedScriptRuntime(){shutdown();}
void ManagedScriptRuntime::initialize(){impl_->open();}
void ManagedScriptRuntime::fixedStep(double delta) {
    if(!std::isfinite(delta) || delta<0)throw std::invalid_argument("Script step requires finite nonnegative delta");
    impl_->synchronize();const auto now=std::chrono::steady_clock::now();
    if(impl_->backend=="coreclr" && now>=impl_->nextPoll){impl_->nextPoll=now+std::chrono::milliseconds(500);impl_->reload();}
    for(auto& entry:impl_->entries)impl_->callback(entry.second,"update",{{"delta",delta}});
}
void ManagedScriptRuntime::dispatch(const PhysicsEvent& event) {
    impl_->open();for(auto entity:{event.trigger,event.other}) {
        const auto found=impl_->entries.find(entity);if(found!=impl_->entries.end())
            impl_->callback(found->second,"trigger",{{"other",impl_->runtime.nodeId(entity==event.trigger?event.other:event.trigger)},{"entered",event.entered}});
    }
}
void ManagedScriptRuntime::dispatchInteraction(const InteractionTarget& event) {
    impl_->open();if(event.revision!=impl_->runtime.sceneRevision() || impl_->runtime.entity(event.node)!=event.target
        || impl_->runtime.entity(event.actorNode)!=event.actor)return;
    const auto found=impl_->entries.find(event.target);if(found!=impl_->entries.end())impl_->callback(found->second,"interact",{{"actor",event.actorNode}});
}
void ManagedScriptRuntime::reloadChanged(){impl_->reload();}
void ManagedScriptRuntime::shutdown() noexcept{impl_->shutdown();}
void ManagedScriptRuntime::setLevelHandler(std::function<void(std::string)> handler){impl_->host->setLevelHandler(std::move(handler));}
void ManagedScriptRuntime::setAudioHandler(std::function<void(ecs::Entity)> handler){impl_->host->setAudioHandler(std::move(handler));}
void ManagedScriptRuntime::setUiHandler(std::function<void(std::string,std::string)> handler){impl_->host->setUiHandler(std::move(handler));}
std::size_t ManagedScriptRuntime::activeCount() const {std::size_t count=0;for(const auto& entry:impl_->entries)if(entry.second.active)++count;return count;}
const std::vector<std::string>& ManagedScriptRuntime::errors() const{return impl_->errors;}
ScriptRuntimeCapabilities ManagedScriptRuntime::capabilities() const{return {impl_->backend,impl_->backend=="coreclr",true};}
}
