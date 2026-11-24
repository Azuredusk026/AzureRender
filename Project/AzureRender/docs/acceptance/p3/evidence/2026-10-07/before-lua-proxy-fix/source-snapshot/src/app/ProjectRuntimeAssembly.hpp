#pragma once
#include "runtime/Project.hpp"
#include "runtime/SystemRegistry.hpp"
#include "runtime/ScriptRuntimeRegistry.hpp"
namespace azurerender::application {
SystemRegistry systems();
nlohmann::json explorationConfiguration();
nlohmann::json configuration(const Project& project);
ScriptRuntimeRegistry scriptBackends();
std::unique_ptr<IScriptRuntime> scripts(const Project&,RuntimeLifecycle&,GameRuntime&,AssetDatabase&);
}
