#include "assets/MaterialData.hpp"
#include <fstream>
#include <iostream>
using namespace azurerender;
int main(int argc,char** argv){try{
    if(argc<2||argc>3)throw std::invalid_argument("Usage: AzureMaterialValidator asset [uuid-paths.json] | --slots model");
    if(std::string(argv[1])=="--describe"){std::cout<<MaterialTypeRegistry::builtins().describe().dump()<<'\n';return 0;}
    if(std::string(argv[1])=="--slots"){if(argc!=3)throw std::invalid_argument("--slots requires a model path");std::cout<<nlohmann::json{{"slots",inspectMaterialSlots(std::filesystem::u8path(argv[2]))}}.dump()<<'\n';return 0;}
    const auto path=std::filesystem::u8path(argv[1]);nlohmann::json output;
    if(path.extension()==".azurematerial"){
        const auto document=MaterialData::load(path);output=document.encode();
        if(argc==3){
            nlohmann::json paths;std::ifstream input(std::filesystem::u8path(argv[2]));input>>paths;
            const auto material=document.interpret([&](const auto& uuid){return std::filesystem::u8path(paths.at(uuid).template get<std::string>());});
            output["validatedClass"]=assetMaterialClassName(material.materialClass);
        }
    }else{
        const auto asset=loadGltfAsset(path.u8string());output={{"materials",nlohmann::json::array()}};
        for(const auto& material:asset.materials)output["materials"].push_back({{"name",material.name},{"class",assetMaterialClassName(material.materialClass)}});
    }
    std::cout<<output.dump()<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
