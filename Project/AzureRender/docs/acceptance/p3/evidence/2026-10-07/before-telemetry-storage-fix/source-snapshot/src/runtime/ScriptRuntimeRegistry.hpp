#pragma once
#include "runtime/IScriptRuntime.hpp"
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
namespace azurerender {
class RuntimeLifecycle;
class GameRuntime;
class AssetDatabase;
class ScriptRuntimeRegistry final {
public:
    using Factory=std::function<std::unique_ptr<IScriptRuntime>(RuntimeLifecycle&,GameRuntime&,AssetDatabase&,const nlohmann::json&)>;
    void add(std::string id,unsigned version,Factory factory);
    std::unique_ptr<IScriptRuntime> create(const nlohmann::json& configuration,RuntimeLifecycle&,GameRuntime&,AssetDatabase&) const;
private:
    std::map<std::string,Factory> factories_;
};
}
