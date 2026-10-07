#include "scripting/dotnet/ManagedScriptingModule.hpp"
#include "scripting/dotnet/ManagedScriptRuntime.hpp"
namespace azurerender {
void registerManagedScripting(ScriptRuntimeRegistry& registry) {
    for(const auto* backend:{"coreclr","nativeaot"})registry.add(backend,1,[](auto& runtime,auto& game,auto& assets,const auto& config) {
        return std::make_unique<ManagedScriptRuntime>(runtime,game,assets,config);
    });
}
}
