#include "editor/EditorSession.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include "assets/GltfLoader.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
int main(){const auto root=std::filesystem::temp_directory_path()/("azure-extended-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
    std::filesystem::create_directories(root);
    const auto svg=root/std::filesystem::u8path(u8"中文图标.svg");{std::ofstream file(svg);file<<"<svg width=\"4\" height=\"4\"><rect width=\"4\" height=\"4\" fill=\"red\"/></svg>";}
    const auto usd=root/"mesh.usda";{std::ofstream file(usd);file<<"#usda 1.0\n( metersPerUnit = 1 )\ndef Mesh \"Triangle\" { uniform token subdivisionScheme = \"none\" point3f[] points = [(0,0,0),(1,0,0),(0,1,0)] int[] faceVertexCounts = [3] int[] faceVertexIndices = [0,1,2] }";}
    for(const auto* type:{"azure.pbr","azure.stylized"}){
        const auto project=root/type;Project::create(project,type);auto context=EditorContext::openProject(project/"project.azureproject");EditorSession session(context);
        auto run=[&](const auto& operation,const Json& args){const auto result=session.edit(operation,args);if(!result)throw std::runtime_error(result.diagnostics.dump());return result.value;};
        const auto image=run("asset.import",{{"path",svg.u8string()}}).get<std::string>();
        const auto model=run("asset.import",{{"path",usd.u8string()}}).get<std::string>();
        const auto material=run("material.create",{{"type",type},{"destination","assets:/surface.azurematerial"}}).at("asset").get<std::string>();
        run("material.field",{{"asset",material},{"field","textures.albedo"},{"value",{{"asset",image},{"usage","albedo"}}}});
        const auto data=MaterialData::load(context->assets().resolve(material));
        const auto rendered=data.interpret([&](const auto& id){return context->assets().resolve(id);});
        if(rendered.baseColorWidth!=4||loadGltfAsset(context->assets().resolve(model).u8string()).indices.size()!=3)throw std::runtime_error("Extended assets must reach actual material and model consumers");
        context->save();auto reopened=EditorContext::openProject(project/"project.azureproject");
        if(!std::filesystem::is_regular_file(reopened->assets().resolve(image)))throw std::runtime_error("Imported extended assets must survive reopening");
    }
    std::filesystem::remove_all(root);std::cout<<"SVG and USDA public editor import, two material consumers and reopen passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
