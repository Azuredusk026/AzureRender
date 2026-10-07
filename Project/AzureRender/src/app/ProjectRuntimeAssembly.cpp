#include "app/ProjectRuntimeAssembly.hpp"
#include "gameplay/SharedSystems.hpp"
#include "gameplay/InteractionPolicy.hpp"
#include "gameplay/SystemConfiguration.hpp"
#include "runtime/GameRuntime.hpp"
#include "scripting/lua/LuaScriptingModule.hpp"
#if AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE
#include "scripting/dotnet/ManagedScriptingModule.hpp"
#endif
namespace azurerender::application {
namespace {
class ExplorationPolicy final: public IRuntimeSystem {
public:
    void initialize(RuntimeSystemContext& c) override {
        c.game.setInteractionPolicy([](const RuntimeLifecycle& runtime,ecs::Entity,ecs::Entity target) {
            const auto* collectible=runtime.world().tryGet<game::Collectible>(target);
            const auto* door=runtime.world().tryGet<game::Door>(target);
            return (!collectible||!collectible->collected)&&(!door||!door->open);
        });
    }
    void fixedStep(RuntimeSystemContext&) override {}
    void shutdown(RuntimeSystemContext& c) noexcept override { c.game.setInteractionPolicy({}); }
};
}
SystemRegistry systems() {
    SystemRegistry registry;gameplay::registerSharedSystems(registry);
    registry.add("exploration-policy",[](const auto& config) { gameplay::fields(config,{});return std::make_unique<ExplorationPolicy>(); });
    return registry;
}
nlohmann::json explorationConfiguration() {
    using Json=nlohmann::json;
    return {{"schemaVersion",1},
        {"input",{{"move-left",{65}},{"move-right",{68}},{"move-forward",{87}},{"move-back",{83}},
            {"jump",{32}},{"sprint",{340,344}},{"interact",{69}},{"restart",{82}}}},
        {"systems",Json::array({
            {{"id","character-movement"},{"config",{{"profiles",{{"default",{{"left","move-left"},{"right","move-right"},{"forward","move-forward"},{"back","move-back"},{"jump","jump"},{"sprint","sprint"}}}}}}}},
            {{"id","physics"},{"config",Json::object()}},
            {{"id","locomotion"},{"config",{{"profiles",{{"default",{{"idle","idle"},{"moving","walk"},{"threshold",.1}}}}}}}},
            {{"id","camera"},{"config",{{"mode","follow"}}}},
            {{"id","exploration-policy"},{"config",Json::object()}},
            {{"id","interaction"},{"config",{{"action","interact"}}}}
        })}};
}
nlohmann::json configuration(const Project& project) {
    return project.runtimeConfiguration.is_null()?explorationConfiguration():project.runtimeConfiguration;
}
ScriptRuntimeRegistry scriptBackends() {
    ScriptRuntimeRegistry registry;registerLuaScripting(registry);
#if AZURE_ENABLE_MANAGED_SCRIPT_PROTOTYPE
    registerManagedScripting(registry);
#endif
    return registry;
}
std::unique_ptr<IScriptRuntime> scripts(const Project& project,RuntimeLifecycle& runtime,GameRuntime& game,AssetDatabase& assets) {
    return scriptBackends().create(project.scriptingConfiguration,runtime,game,assets);
}
}
