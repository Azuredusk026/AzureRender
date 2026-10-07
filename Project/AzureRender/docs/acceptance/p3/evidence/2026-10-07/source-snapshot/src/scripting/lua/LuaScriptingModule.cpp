#include "scripting/lua/LuaScriptingModule.hpp"
#include "runtime/ScriptRuntimeRegistry.hpp"
#include "runtime/ScriptRuntime.hpp"
#include <stdexcept>
namespace azurerender {
void registerLuaScripting(ScriptRuntimeRegistry& registry) {
    registry.add("lua",1,[](auto& runtime,auto& game,auto& assets,const auto& config) {
        if(config.size()!=2)throw std::invalid_argument("Lua scripting configuration accepts version and backend");
        return std::make_unique<ScriptRuntime>(runtime,game,assets);
    });
}
}
