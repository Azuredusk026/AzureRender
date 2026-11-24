#pragma once
#include "runtime/GameRuntime.hpp"
#include "scripting/BindingDescriptor.hpp"
#include <functional>
#include <map>
#include <set>
#include <thread>
namespace azurerender {
struct ScriptObject {
    ecs::EntityHandle identity;
    std::string node;
    std::uint64_t revision=0;
};
// The language adapters share guards, component metadata and init transactions.
// Callbacks execute on the owner thread; deferred effects retain lifecycle IDs.
class ScriptBindingHost final {
public:
    ScriptBindingHost(RuntimeLifecycle& runtime,GameRuntime& game):runtime_(runtime),game_(game) {}
    ScriptObject object(const std::string& node);
    bool valid(const ScriptObject& object);
    nlohmann::json invoke(const ScriptObject&,const std::string&,const nlohmann::json&);
    void beginCallback(bool initialization=false);
    void endCallback(bool success);
    void commitInitialization(const std::function<void()>& retirement={});
    void cancelInitialization() noexcept;
    void close() noexcept;
    void setLevelHandler(std::function<void(std::string)> handler);
    void setAudioHandler(std::function<void(ecs::Entity)> handler);
    void setUiHandler(std::function<void(std::string,std::string)> handler);
    static nlohmann::json encode(const ScriptObject&);
    static ScriptObject decode(const nlohmann::json&);
private:
    void checkThread() const;
    void guard(const ScriptObject&,bool mutating);
    void effect(std::function<void()>,bool structural=false);
    RuntimeLifecycle& runtime_;
    GameRuntime& game_;
    const std::thread::id owner_=std::this_thread::get_id();
    bool closed_=false,callback_=false,staging_=false,pending_=false;
    using Key=std::pair<ecs::Entity,std::string>;
    std::map<Key,std::pair<ScriptObject,nlohmann::json>> stagedComponents_;
    std::vector<std::function<void()>> stagedEffects_;
    std::vector<std::function<void()>> stagedStructural_;
    std::set<std::string> stagedSpawns_;
    std::function<void(std::string)> level_;
    std::function<void(ecs::Entity)> audio_;
    std::function<void(std::string,std::string)> ui_;
};
}
