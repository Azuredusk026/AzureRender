#include "BuiltinGeometryService.hpp"
#include "assets/GltfLoader.hpp"
namespace azurerender {
std::string ensureBuiltinGeometry(EditorContext& context,const std::string& kind,const std::array<float,3>& dimensions) {
    if(!context.isProject())throw std::invalid_argument("Geometry creation requires a project");
    const auto mount=context.project().mounts.begin()->first;
    const auto output=mount+":/Generated/Primitives/"+kind+"-v1.glb";
    const nlohmann::json parameters={{"kind",kind},{"size",dimensions},{"segments",24}};
    if(std::filesystem::exists(context.project().resolve(output))) {
        const auto id=context.assets().idForPath(output);const auto& asset=context.assets().records().at(id);
        if(!asset.generation||asset.generation->generatorId!="azure.primitive"||asset.generation->parametersHash!=generationHash(parameters.dump()))
            throw std::invalid_argument("Built-in geometry destination contains different content");
        static_cast<void>(loadGltfAsset(asset.path.u8string()));return id;
    }
    return context.assets().generateAsset(GeneratorRegistry::builtins(),"azure.primitive",output,parameters,{}, {},
        "Azure primitive generator v1",[](const std::string&){ });
}
}
