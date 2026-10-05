#define SOL_ALL_SAFETIES_ON 1
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 5321)
#endif
#include <sol/sol.hpp>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "runtime/ScriptRuntime.hpp"
#include "runtime/ComponentCodec.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <chrono>
#include <fstream>
#include <set>
namespace azurerender {
namespace {
using Json = nlohmann::json;
Json json(const sol::object& value, unsigned depth = 0) {
    if (depth > 16) throw std::invalid_argument("Lua value nesting exceeds limit");
    switch (value.get_type()) {
    case sol::type::boolean: return value.as<bool>();
    case sol::type::number: {
        const double number = value.as<double>();
        if (!std::isfinite(number)) throw std::invalid_argument("Lua numeric property must be finite");
        if (std::floor(number) == number && number >= -2147483648.0 && number <= 4294967295.0) return static_cast<std::int64_t>(number);
        return number;
    }
    case sol::type::string: return value.as<std::string>();
    case sol::type::table: {
        const sol::table table = value;
        Json array = Json::array();
        if (table.size() > 1024) throw std::invalid_argument("Lua array exceeds limit");
        for (std::size_t index = 1; index <= table.size(); ++index) array.push_back(json(table.get<sol::object>(index), depth + 1));
        if (array.size() != static_cast<std::size_t>(std::distance(table.begin(), table.end()))) throw std::invalid_argument("Expected dense Lua array");
        return array;
    }
    default: throw std::invalid_argument("Unsupported reflected Lua value");
    }
}
sol::object luaValue(sol::state& lua, const Json& value) {
    if (value.is_boolean()) return sol::make_object(lua, value.get<bool>());
    if (value.is_number()) return sol::make_object(lua, value.get<double>());
    if (value.is_string()) return sol::make_object(lua, value.get<std::string>());
    if (value.is_array()) {
        auto table = lua.create_table();
        unsigned index = 1; for (const auto& entry : value) table[index++] = luaValue(lua, entry);
        return sol::make_object(lua, table);
    }
    return sol::make_object(lua, sol::nil);
}
void instructionLimit(lua_State* state, lua_Debug*) { luaL_error(state, "Script instruction budget exceeded"); }
}
struct ScriptRuntime::Impl {
    RuntimeLifecycle& runtime;
    GameRuntime& game;
    AssetDatabase& assets;
    const reflection::Registry& registry = runtimeComponentRegistry().metadata();
    sol::state lua;
    struct Entry {
        ecs::Entity entity;
        std::string node, asset, attemptedSource;
        std::uint64_t revision;
        sol::environment environment;
        bool active = true;
    };
    std::map<ecs::Entity, Entry> entries;
    std::vector<std::string> errors;
    std::function<void(std::string)> levelHandler;
    std::function<void(ecs::Entity)> audioHandler;
    std::function<void(std::string,std::string)> uiHandler;
    std::chrono::steady_clock::time_point nextPoll{};
    bool callbackActive = false;
    bool staging = false;
    std::map<std::pair<ecs::Entity,std::string>, Json> stagedComponents;
    std::vector<std::function<void()>> stagedEffects;
    std::set<std::string> stagedSpawns;
    std::uint64_t sceneRevision = 0;
    Impl(RuntimeLifecycle& r, GameRuntime& g, AssetDatabase& a) : runtime(r), game(g), assets(a) {
        lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);
    }
    ~Impl() {
        for (auto& entry : entries)
            if (entry.second.active && alive(entry.first, entry.second.node, entry.second.revision))
                call(entry.second, "shutdown");
    }
    void error(const std::string& message) {
        if (errors.empty() || errors.back() != message) {
            if (errors.size() == 128) errors.erase(errors.begin());
            errors.push_back(message); RuntimeDiagnostics::instance().warning("script", message);
        }
    }
    bool alive(ecs::Entity entity, const std::string& node, std::uint64_t revision) {
        return runtime.sceneRevision() == revision && runtime.entity(node) == entity && runtime.world().valid(entity);
    }
    void guard(ecs::Entity entity, const std::string& node, std::uint64_t revision, bool mutating) {
        if (!alive(entity, node, revision)) throw std::runtime_error("Script entity handle is stale");
        if (mutating && !callbackActive) throw std::runtime_error("Gameplay mutation requires a runtime callback");
    }
    sol::environment environment(ecs::Entity entity, const std::string& node, std::uint64_t revision) {
        sol::environment env(lua, sol::create);
        for (const char* name : {"assert", "error", "ipairs", "pairs", "next", "select", "tonumber", "tostring", "type"}) env[name] = lua[name];
        for (const char* library : {"math", "string", "table"}) {
            auto copy = lua.create_table(); const sol::table source = lua[library];
            for (const auto& entry : source) copy[entry.first] = entry.second;
            env[library] = copy;
        }
        env["_G"] = env;
        auto self = lua.create_table(); self["id"] = node;
        self.set_function("find",[this,entity,node,revision](sol::table,const std::string& id)->sol::object {
            guard(entity,node,revision,false);
            const auto target=runtime.entity(id);
            if(target==ecs::kInvalidEntity)return sol::make_object(lua,sol::nil);
            auto targetEnvironment=environment(target,id,revision);
            return targetEnvironment["self"].get<sol::object>();
        });
        self.set_function("interaction_target",[this,entity,node,revision](sol::table){
            guard(entity,node,revision,false);const auto& target=game.interactionTarget();
            return target&&target->revision==revision&&target->actor==entity&&runtime.entity(target->node)==target->target?target->node:std::string();
        });
        self.set_function("has",[this,entity,node,revision](sol::table,const std::string& type){
            guard(entity,node,revision,false);
            return runtimeComponentRegistry().contains(type, runtime.world(), entity);
        });
        self.set_function("remove_component",[this,entity,node,revision](sol::table,const std::string& type){
            guard(entity,node,revision,true);
            if(type=="azure.transform"||type=="azure.renderable")throw std::invalid_argument("Structural identity components are required");
            // Resolve registration before scheduling, so unknown types fail now.
            static_cast<void>(runtimeComponentRegistry().describe(type));
            auto effect=[this,entity,node,revision,type]{auto* lifecycle=&runtime;
                runtime.defer([lifecycle,entity,node,revision,type](auto& world){
                    if(lifecycle->sceneRevision()==revision&&lifecycle->entity(node)==entity)
                        runtimeComponentRegistry().remove(type,world,entity);});};
            if(staging)stagedEffects.push_back(effect);else effect();
        });
        self.set_function("spawn",[this,entity,node,revision](sol::table,const std::string& id,const std::string& resource,sol::object position){
            guard(entity,node,revision,true);SceneNode spawned;spawned.id=id;spawned.resourceId=resource;spawned.visible=!resource.empty();
            ecs::TransformComponent defaults;auto data=registry.encode("azure.transform",&defaults);
            data["data"]["translation"]=json(position);ecs::TransformComponent transform;registry.decode("azure.transform",&transform,data);
            spawned.translation=transform.translation;
            runtime.validateSpawn(spawned);
            if(staging&&!stagedSpawns.insert(id).second)throw std::invalid_argument("Duplicate staged spawn identity");
            auto effect=[this,spawned]{runtime.deferSpawn(spawned);};
            if(staging)stagedEffects.push_back(effect);else effect();
        });
        self.set_function("alive", [this, entity, node, revision](sol::table) { return alive(entity, node, revision); });
        self.set_function("action", [this, entity, node, revision](sol::table, const std::string& action) {
            guard(entity, node, revision, false); return game.input().down(action);
        });
        self.set_function("pressed", [this, entity, node, revision](sol::table, const std::string& action) {
            guard(entity, node, revision, false); return game.input().pressed(action);
        });
        self.set_function("move", [this, entity, node, revision](sol::table, float x, float z, bool jump) {
            guard(entity, node, revision, true);
            if (!std::isfinite(x) || !std::isfinite(z)) throw std::invalid_argument("Character motion must be finite");
            auto effect = [this, entity, x, z, jump] { game.move(entity, {x, z, jump}); };
            if (staging) stagedEffects.push_back(effect); else effect();
        });
        self.set_function("destroy", [this, entity, node, revision](sol::table) {
            guard(entity, node, revision, true);
            auto effect = [this, entity, node, revision] {
                auto* lifecycle = &runtime;
                runtime.defer([lifecycle, entity, node, revision](auto& world) {
                    if (lifecycle->sceneRevision() == revision && lifecycle->entity(node) == entity && world.valid(entity))
                        world.destroyEntity(entity);
                });
            };
            if (staging) stagedEffects.push_back(effect); else effect();
        });
        self.set_function("load_level", [this, entity, node, revision](sol::table, const std::string& reference) {
            guard(entity, node, revision, true);
            if (!levelHandler) throw std::runtime_error("Level request handler unavailable");
            auto effect = [this, reference] { levelHandler(reference); };
            if (staging) stagedEffects.push_back(effect); else effect();
        });
        self.set_function("audio_play", [this,entity,node,revision](sol::table){
            guard(entity,node,revision,true);if(!audioHandler)throw std::runtime_error("Audio handler unavailable");
            auto effect=[this,entity]{audioHandler(entity);};if(staging)stagedEffects.push_back(effect);else effect();
        });
        self.set_function("ui_text", [this,entity,node,revision](sol::table,const std::string& id,const std::string& text){
            guard(entity,node,revision,true);if(!uiHandler)throw std::runtime_error("UI handler unavailable");
            auto effect=[this,id,text]{uiHandler(id,text);};if(staging)stagedEffects.push_back(effect);else effect();
        });
        self.set_function("get", [this, entity, node, revision](sol::table, const std::string& type, const std::string& field) {
            guard(entity, node, revision, false);
            const auto key=std::make_pair(entity,type);
            const auto data = staging && stagedComponents.count(key) ? stagedComponents.at(key)
                : runtimeComponentRegistry().encode(type,runtime.world(),entity);
            return luaValue(lua, data.at("data").at(field));
        });
        self.set_function("set", [this, entity, node, revision](sol::table, const std::string& type, const std::string& field, sol::object input) {
            guard(entity, node, revision, true);
            const auto value = json(input);
            auto& components = runtimeComponentRegistry();
            components.validateWrite(type,field,value,false);
            const auto key=std::make_pair(entity,type);
            auto data = staging && stagedComponents.count(key) ? stagedComponents.at(key)
                : components.encode(type,runtime.world(),entity);
            data["data"][field] = value;
            components.validate(type,data);
            if(staging)stagedComponents[key]=data;
            else components.install(type,runtime.world(),entity,data);
        });
        env["self"] = self;
        return env;
    }
    std::string source(const std::string& reference) {
        std::ifstream file(assets.resolveReference(reference), std::ios::binary);
        if (!file) throw std::runtime_error("Cannot read Lua asset: " + reference);
        std::string result{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        if (result.size() > 1024 * 1024) throw std::runtime_error("Lua source exceeds 1 MiB");
        return result;
    }
    template<class... Args> bool call(Entry& entry, const std::string& name, Args&&... args) {
        sol::object function = entry.environment[name];
        if (function.get_type() == sol::type::nil) return true;
        if (function.get_type() != sol::type::function) { error(entry.asset + ": callback is not a function: " + name); entry.active = false; return false; }
        sol::protected_function callback = function;
        callbackActive = true; lua_sethook(lua.lua_state(), instructionLimit, LUA_MASKCOUNT, 100000);
        const sol::protected_function_result result = callback(std::forward<Args>(args)...);
        lua_sethook(lua.lua_state(), nullptr, 0, 0); callbackActive = false;
        if (!result.valid()) {
            const sol::error failure = result; error(entry.asset + ": " + failure.what()); entry.active = false; return false;
        }
        return true;
    }
    bool reload(ecs::Entity entity, const std::string& asset, const std::string& code) {
        auto existing = entries.find(entity);
        if (existing != entries.end()) existing->second.attemptedSource = code;
        const std::string node = runtime.nodeId(entity); const auto revision = runtime.sceneRevision();
        auto env = environment(entity, node, revision);
        const sol::load_result loaded = lua.load(code, "@" + assets.resolveReference(asset).generic_string());
        if (!loaded.valid()) { const sol::error failure = loaded; error(asset + ": " + failure.what()); return false; }
        sol::protected_function chunk = loaded; sol::set_environment(env, chunk);
        lua_sethook(lua.lua_state(), instructionLimit, LUA_MASKCOUNT, 100000);
        const sol::protected_function_result result = chunk(); lua_sethook(lua.lua_state(), nullptr, 0, 0);
        if (!result.valid()) { const sol::error failure = result; error(asset + ": " + failure.what()); return false; }
        for (const char* name : {"init", "update", "trigger", "interact", "shutdown"}) {
            const sol::object callback = env[name];
            if (callback.get_type() != sol::type::nil && callback.get_type() != sol::type::function) {
                error(asset + ": invalid callback: " + name); return false;
            }
        }
        Entry candidate{entity, node, asset, code, revision, std::move(env), true};
        stagedComponents.clear(); stagedEffects.clear(); stagedSpawns.clear(); staging = true;
        const bool initialized = call(candidate, "init"); staging = false;
        if (!initialized) { stagedComponents.clear(); stagedEffects.clear(); stagedSpawns.clear(); return false; }
        if (existing != entries.end() && existing->second.active) call(existing->second, "shutdown");
        for (const auto& component : stagedComponents)
            if(runtimeComponentRegistry().contains(component.first.second,runtime.world(),component.first.first))
                runtimeComponentRegistry().install(component.first.second,runtime.world(),component.first.first,component.second);
        for (const auto& effect : stagedEffects) effect();
        stagedComponents.clear(); stagedEffects.clear(); stagedSpawns.clear();
        entries.insert_or_assign(entity, std::move(candidate));
        return true;
    }
    void synchronize() {
        if (sceneRevision != runtime.sceneRevision()) { entries.clear(); sceneRevision = runtime.sceneRevision(); }
        for (auto it = entries.begin(); it != entries.end();) {
            auto* component = runtime.world().tryGet<game::Script>(it->first);
            if (!component || !component->enabled || !alive(it->first, it->second.node, it->second.revision)) {
                if (it->second.active && alive(it->first, it->second.node, it->second.revision)) call(it->second, "shutdown");
                it = entries.erase(it);
            }
            else ++it;
        }
        runtime.world().each<game::Script>([&](auto entity, const auto& script) {
            if (!script.enabled || script.asset.empty()) return;
            const auto found = entries.find(entity);
            if (found == entries.end() || found->second.asset != script.asset) {
                try {
                    const auto code = source(script.asset);
                    if (!reload(entity, script.asset, code) && !entries.count(entity))
                        entries.emplace(entity, Entry{entity, runtime.nodeId(entity), script.asset, code, runtime.sceneRevision(), environment(entity, runtime.nodeId(entity), runtime.sceneRevision()), false});
                } catch (const std::exception& failure) {
                    error(script.asset + ": " + failure.what());
                    if (!entries.count(entity)) entries.emplace(entity, Entry{entity, runtime.nodeId(entity), script.asset, {}, runtime.sceneRevision(), environment(entity, runtime.nodeId(entity), runtime.sceneRevision()), false});
                }
            }
        });
    }
    void reloadChanged() {
        synchronize();
        for (auto& entry : entries) {
            try { const auto code = source(entry.second.asset); if (code != entry.second.attemptedSource) reload(entry.first, entry.second.asset, code); }
            catch (const std::exception& failure) { error(entry.second.asset + ": " + failure.what()); }
        }
    }
};
ScriptRuntime::ScriptRuntime(RuntimeLifecycle& runtime, GameRuntime& game, AssetDatabase& assets)
    : impl_(std::make_unique<Impl>(runtime, game, assets)) {}
ScriptRuntime::~ScriptRuntime() = default;
void ScriptRuntime::update(double delta) {
    impl_->synchronize();
    const auto now = std::chrono::steady_clock::now();
    if (now >= impl_->nextPoll) { impl_->nextPoll = now + std::chrono::milliseconds(500); impl_->reloadChanged(); }
    for (auto& entry : impl_->entries) if (entry.second.active) impl_->call(entry.second, "update", delta);
}
void ScriptRuntime::dispatch(const PhysicsEvent& event) {
    for (auto entity : {event.trigger, event.other}) {
        const auto found = impl_->entries.find(entity);
        if (found != impl_->entries.end() && found->second.active) impl_->call(found->second, "trigger", impl_->runtime.nodeId(entity == event.trigger ? event.other : event.trigger), event.entered);
    }
}
void ScriptRuntime::reloadChanged() { impl_->reloadChanged(); }
void ScriptRuntime::dispatchInteraction(const InteractionTarget& event) {
    if(event.revision!=impl_->runtime.sceneRevision()||impl_->runtime.entity(event.node)!=event.target
        ||impl_->runtime.entity(event.actorNode)!=event.actor)return;
    const auto found=impl_->entries.find(event.target);
    if(found!=impl_->entries.end()&&found->second.active)impl_->call(found->second,"interact",event.actorNode);
}
void ScriptRuntime::setLevelHandler(std::function<void(std::string)> handler) { impl_->levelHandler = std::move(handler); }
void ScriptRuntime::setAudioHandler(std::function<void(ecs::Entity)> handler) { impl_->audioHandler = std::move(handler); }
void ScriptRuntime::setUiHandler(std::function<void(std::string,std::string)> handler) { impl_->uiHandler = std::move(handler); }
std::size_t ScriptRuntime::activeCount() const {
    std::size_t count = 0; for (const auto& entry : impl_->entries) count += entry.second.active; return count;
}
const std::vector<std::string>& ScriptRuntime::errors() const { return impl_->errors; }
} // namespace azurerender
