#include "runtime/ScriptRuntimeRegistry.hpp"
#include <stdexcept>
namespace azurerender {
void ScriptRuntimeRegistry::add(std::string id,unsigned version,Factory factory) {
    if(id.empty() || version!=1 || !factory || factories_.count(id))throw std::invalid_argument("Invalid or duplicate script backend registration");
    factories_.emplace(std::move(id),std::move(factory));
}
std::unique_ptr<IScriptRuntime> ScriptRuntimeRegistry::create(const nlohmann::json& configuration,
    RuntimeLifecycle& runtime,GameRuntime& game,AssetDatabase& assets) const {
    if(!configuration.is_object() || !configuration.at("schemaVersion").is_number_integer()
        || configuration.at("schemaVersion")!=1 || !configuration.at("backend").is_string())
        throw std::invalid_argument("Unsupported scripting configuration");
    const auto id=configuration.at("backend").get<std::string>();
    const auto found=factories_.find(id);
    if(found==factories_.end())throw std::invalid_argument("Script backend unavailable: "+id);
    auto service=found->second(runtime,game,assets,configuration);
    if(!service || service->capabilities().backend!=id)throw std::runtime_error("Script backend factory contract mismatch");
    service->initialize();return service;
}
}
