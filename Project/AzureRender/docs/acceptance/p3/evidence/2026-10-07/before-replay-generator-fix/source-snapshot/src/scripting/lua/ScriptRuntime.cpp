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
#include "scripting/GeneratedBindings.hpp"
#include "scripting/ScriptBindingHost.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
namespace azurerender {
namespace {
using Json=nlohmann::json;
Json json(const sol::object& value,unsigned depth=0) {
    if(depth>16)throw std::invalid_argument("Lua value nesting exceeds limit");
    switch(value.get_type()) {
    case sol::type::boolean:return value.as<bool>();
    case sol::type::number: {
        const double number=value.as<double>();
        if(!std::isfinite(number))throw std::invalid_argument("Lua numeric property must be finite");
        if(std::floor(number)==number && number>=-2147483648.0 && number<=4294967295.0)
            return static_cast<std::int64_t>(number);
        return number;
    }
    case sol::type::string:return value.as<std::string>();
    case sol::type::table: {
        const sol::table table=value;Json array=Json::array();
        if(table.size()>1024)throw std::invalid_argument("Lua array exceeds limit");
        for(std::size_t index=1;index<=table.size();++index)array.push_back(json(table.get<sol::object>(index),depth+1));
        if(array.size()!=static_cast<std::size_t>(std::distance(table.begin(),table.end())))
            throw std::invalid_argument("Expected dense Lua array");
        return array;
    }
    default:throw std::invalid_argument("Unsupported reflected Lua value");
    }
}
sol::object luaValue(sol::state& lua,const Json& value) {
    if(value.is_boolean())return sol::make_object(lua,value.get<bool>());
    if(value.is_number())return sol::make_object(lua,value.get<double>());
    if(value.is_string())return sol::make_object(lua,value.get<std::string>());
    if(value.is_array()) {
        auto table=lua.create_table();unsigned index=1;
        for(const auto& entry:value)table[index++]=luaValue(lua,entry);
        return sol::make_object(lua,table);
    }
    return sol::make_object(lua,sol::nil);
}
void instructionLimit(lua_State* state,lua_Debug*) {luaL_error(state,"Script instruction budget exceeded");}
}
struct ScriptRuntime::Impl {
    RuntimeLifecycle& runtime;
    AssetDatabase& assets;
    ScriptBindingHost host;
    sol::state lua;
    struct Entry {
        ScriptObject object;
        std::string asset,attemptedSource;
        sol::environment environment;
        bool active=true;
    };
    std::map<ecs::Entity,Entry> entries;
    std::vector<std::string> errors;
    std::chrono::steady_clock::time_point nextPoll{};
    std::uint64_t sceneRevision=0;
    bool closed=false;
    Impl(RuntimeLifecycle& r,GameRuntime& g,AssetDatabase& a):runtime(r),assets(a),host(r,g) {
        lua.open_libraries(sol::lib::base,sol::lib::math,sol::lib::string,sol::lib::table);
    }
    void requireOpen() {if(closed)throw std::logic_error("Script runtime is closed");}
    void error(const std::string& message) {
        if(errors.empty() || errors.back()!=message) {
            if(errors.size()==128)errors.erase(errors.begin());
            errors.push_back(message);RuntimeDiagnostics::instance().warning("script",message);
        }
    }
    sol::table objectTable(const ScriptObject& object) {
        auto backing=lua.create_table();auto proxy=lua.create_table();
        for(const auto& binding:kScriptBindings) {
            const std::string method=binding.name;
            backing.set_function(method,[this,object,method](sol::variadic_args input)->sol::object {
                if(input.size()<1 || input[0].get<sol::object>().get_type()!=sol::type::table)
                    throw std::invalid_argument("Lua object methods require colon invocation");
                Json args=Json::array();
                for(std::size_t index=1;index<input.size();++index)args.push_back(json(input[index].get<sol::object>()));
                const auto result=host.invoke(object,method,args);
                if(method=="find" && !result.is_null())return sol::make_object(lua,objectTable(ScriptBindingHost::decode(result)));
                return luaValue(lua,result);
            });
        }
        auto metadata=lua.create_table();
        metadata.set_function("__index",[this,object,backing](sol::table,const std::string& key)->sol::object {
            if(key=="id")return luaValue(lua,host.invoke(object,"id",Json::array()));
            return backing[key].get<sol::object>();
        });
        metadata.set_function("__newindex",[backing](sol::table,const std::string& key,sol::object value)mutable {
            const bool method=std::any_of(std::begin(kScriptBindings),std::end(kScriptBindings),
                [&key](const auto& binding){return key==binding.name;});
            if(key=="id" || method)
                throw std::invalid_argument("Script object API fields are read only");
            backing[key]=value;
        });
        proxy[sol::metatable_key]=metadata;
        return proxy;
    }
    sol::environment environment(const ScriptObject& object) {
        sol::environment env(lua,sol::create);
        for(const char* name:{"assert","error","ipairs","pairs","next","select","tonumber","tostring","type"})env[name]=lua[name];
        for(const char* library:{"math","string","table"}) {
            auto copy=lua.create_table();const sol::table source=lua[library];
            for(const auto& entry:source)copy[entry.first]=entry.second;
            env[library]=copy;
        }
        env["_G"]=env;env["self"]=objectTable(object);return env;
    }
    std::string source(const std::string& reference) {
        std::ifstream file(assets.resolveReference(reference),std::ios::binary);
        if(!file)throw std::runtime_error("Cannot read Lua asset: "+reference);
        std::string result{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        if(result.size()>1024*1024)throw std::runtime_error("Lua source exceeds one MiB");
        return result;
    }
    template<class... Args> bool call(Entry& entry,const std::string& name,Args&&... args) {
        const sol::object function=entry.environment[name];
        if(function.get_type()!=sol::type::nil && function.get_type()!=sol::type::function) {
            error(entry.asset+": callback is not a function: "+name);entry.active=false;return false;
        }
        host.beginCallback(name=="init");
        if(function.get_type()==sol::type::nil){host.endCallback(true);return true;}
        sol::protected_function callback=function;
        lua_sethook(lua.lua_state(),instructionLimit,LUA_MASKCOUNT,100000);
        const sol::protected_function_result result=callback(std::forward<Args>(args)...);
        lua_sethook(lua.lua_state(),nullptr,0,0);host.endCallback(result.valid());
        if(!result.valid()) {
            const sol::error failure=result;error(entry.asset+": "+failure.what());entry.active=false;return false;
        }
        return true;
    }
    bool reload(ecs::Entity entity,const std::string& asset,const std::string& code) {
        auto existing=entries.find(entity);
        if(existing!=entries.end())existing->second.attemptedSource=code;
        const auto object=host.object(runtime.nodeId(entity));auto env=environment(object);
        const sol::load_result loaded=lua.load(code,"@"+assets.resolveReference(asset).generic_string());
        if(!loaded.valid()){const sol::error failure=loaded;error(asset+": "+failure.what());return false;}
        sol::protected_function chunk=loaded;sol::set_environment(env,chunk);
        lua_sethook(lua.lua_state(),instructionLimit,LUA_MASKCOUNT,100000);
        const sol::protected_function_result result=chunk();lua_sethook(lua.lua_state(),nullptr,0,0);
        if(!result.valid()){const sol::error failure=result;error(asset+": "+failure.what());return false;}
        for(const char* name:{"init","update","trigger","interact","shutdown"}) {
            const sol::object callback=env[name];
            if(callback.get_type()!=sol::type::nil && callback.get_type()!=sol::type::function) {
                error(asset+": invalid callback: "+name);return false;
            }
        }
        Entry candidate{object,asset,code,std::move(env),true};
        if(!call(candidate,"init"))return false;
        host.commitInitialization([&] {
            if(existing!=entries.end() && existing->second.active)call(existing->second,"shutdown");
        });
        entries.insert_or_assign(entity,std::move(candidate));return true;
    }
    void synchronize() {
        requireOpen();
        if(sceneRevision!=runtime.sceneRevision()){entries.clear();sceneRevision=runtime.sceneRevision();}
        for(auto it=entries.begin();it!=entries.end();) {
            const auto* component=runtime.world().tryGet<game::Script>(it->first);
            if(!component || !component->enabled || !host.valid(it->second.object)) {
                if(it->second.active && host.valid(it->second.object))call(it->second,"shutdown");
                it=entries.erase(it);
            }else ++it;
        }
        runtime.world().each<game::Script>([&](auto entity,const auto& script) {
            if(!script.enabled || script.asset.empty())return;
            const auto found=entries.find(entity);
            if(found==entries.end() || found->second.asset!=script.asset) {
                try {
                    const auto code=source(script.asset);
                    if(!reload(entity,script.asset,code) && !entries.count(entity))
                        entries.emplace(entity,Entry{host.object(runtime.nodeId(entity)),script.asset,code,
                            environment(host.object(runtime.nodeId(entity))),false});
                }catch(const std::exception& failure) {
                    host.cancelInitialization();error(script.asset+": "+failure.what());
                    if(!entries.count(entity))entries.emplace(entity,Entry{host.object(runtime.nodeId(entity)),script.asset,{},
                        environment(host.object(runtime.nodeId(entity))),false});
                }
            }
        });
    }
    void reloadChanged() {
        synchronize();
        for(auto& entry:entries) {
            try{const auto code=source(entry.second.asset);if(code!=entry.second.attemptedSource)reload(entry.first,entry.second.asset,code);}
            catch(const std::exception& failure){host.cancelInitialization();error(entry.second.asset+": "+failure.what());}
        }
    }
    void shutdown() noexcept {
        if(closed)return;
        for(auto& entry:entries)try {
            if(entry.second.active && host.valid(entry.second.object))call(entry.second,"shutdown");
        }catch(...){host.cancelInitialization();}
        entries.clear();host.close();closed=true;
    }
};
ScriptRuntime::ScriptRuntime(RuntimeLifecycle& runtime,GameRuntime& game,AssetDatabase& assets)
    :impl_(std::make_unique<Impl>(runtime,game,assets)) {initialize();}
ScriptRuntime::~ScriptRuntime(){shutdown();}
void ScriptRuntime::initialize(){impl_->requireOpen();}
void ScriptRuntime::fixedStep(double delta) {
    if(!std::isfinite(delta) || delta<0)throw std::invalid_argument("Script step requires a finite nonnegative delta");
    impl_->synchronize();const auto now=std::chrono::steady_clock::now();
    if(now>=impl_->nextPoll){impl_->nextPoll=now+std::chrono::milliseconds(500);impl_->reloadChanged();}
    for(auto& entry:impl_->entries)if(entry.second.active)impl_->call(entry.second,"update",delta);
}
void ScriptRuntime::dispatch(const PhysicsEvent& event) {
    impl_->requireOpen();
    for(auto entity:{event.trigger,event.other}) {
        const auto found=impl_->entries.find(entity);
        if(found!=impl_->entries.end() && found->second.active)
            impl_->call(found->second,"trigger",impl_->runtime.nodeId(entity==event.trigger?event.other:event.trigger),event.entered);
    }
}
void ScriptRuntime::reloadChanged(){impl_->reloadChanged();}
void ScriptRuntime::dispatchInteraction(const InteractionTarget& event) {
    impl_->requireOpen();
    if(event.revision!=impl_->runtime.sceneRevision() || impl_->runtime.entity(event.node)!=event.target
        || impl_->runtime.entity(event.actorNode)!=event.actor)return;
    const auto found=impl_->entries.find(event.target);
    if(found!=impl_->entries.end() && found->second.active)impl_->call(found->second,"interact",event.actorNode);
}
void ScriptRuntime::shutdown() noexcept {impl_->shutdown();}
void ScriptRuntime::setLevelHandler(std::function<void(std::string)> handler){impl_->host.setLevelHandler(std::move(handler));}
void ScriptRuntime::setAudioHandler(std::function<void(ecs::Entity)> handler){impl_->host.setAudioHandler(std::move(handler));}
void ScriptRuntime::setUiHandler(std::function<void(std::string,std::string)> handler){impl_->host.setUiHandler(std::move(handler));}
std::size_t ScriptRuntime::activeCount() const {
    std::size_t result=0;for(const auto& entry:impl_->entries)if(entry.second.active)++result;return result;
}
const std::vector<std::string>& ScriptRuntime::errors() const{return impl_->errors;}
}
