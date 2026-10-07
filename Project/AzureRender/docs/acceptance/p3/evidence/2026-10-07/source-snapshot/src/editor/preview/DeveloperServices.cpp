#include "editor/preview/DeveloperServices.hpp"
#include "editor/EditorSession.hpp"
#include "editor/commands/EditRegistry.hpp"
namespace azurerender {
void registerDeveloperOperations(EditRegistry& registry,EditorSession& session){
    using Json=nlohmann::json;
    const Json text={{"type","string"}},handle={{"type","integer"},{"minimum",1}},vector={{"type","array"},{"minItems",3},{"maxItems",3},{"items",{{"type","number"}}}};
    const Json grade={{"type","object"},{"properties",{{"exposureEv",{{"type","number"}}},{"saturation",{{"type","number"}}},
        {"contrast",{{"type","number"}}},{"tint",vector},{"toneMappingEnabled",{{"type","boolean"}}}}}};
    const auto add=[&](const std::string& name,Json properties,Json required=Json::array()){
        registry.add({name,1,{{"type","object"},{"properties",std::move(properties)},{"required",std::move(required)}},false,false,false},
            [&session,name](EditorContext&,const Json& parameters){
                const auto& provider=session.developerServices().operation;
                if(!provider)throw EditRejection("Developer services are unavailable");
                return provider(name,parameters);
            });
    };
    add("developer.shader-rebuild",Json::object());add("developer.shader-cancel",Json::object());add("developer.describe",Json::object());
    const Json extent={{"type","array"},{"minItems",2},{"maxItems",2},{"items",{{"type","integer"},{"minimum",1},{"maximum",4096}}}};
    add("preview.create",{{"renderer",text},{"extent",extent},{"position",vector},{"target",vector},{"grade",grade}});
    add("preview.request",{{"handle",handle}},{"handle"});add("preview.release",{{"handle",handle}},{"handle"});
    add("preview.capture",{{"handle",handle},{"label",text}},{"handle","label"});
    add("preview.inspect",{{"handle",handle}},{"handle"});
    add("preview.resize",{{"handle",handle},{"extent",extent}},{"handle","extent"});
    add("preview.camera",{{"enabled",{{"type","boolean"}}},{"position",vector},{"target",vector}},{"enabled"});
    add("preview.asset",{{"asset",text}},{"asset"});add("preview.clear",Json::object());
}
}
