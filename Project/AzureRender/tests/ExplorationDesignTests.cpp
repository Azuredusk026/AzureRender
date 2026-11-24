#include "runtime/Project.hpp"
#include "runtime/AssetDatabase.hpp"
#include "assets/GltfLoader.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <cmath>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){if(argc!=2)return 2;try{
    const auto project=Project::load(argv[1]);AssetDatabase assets(project);assets.refresh();
    const auto level=project.loadStartupScene();require(!level.nodes.empty(),"Exploration is a valid runtime level");
    std::ifstream input(project.file.parent_path()/"EXPLORATION-DESIGN.json");nlohmann::json design;input>>design;
    require(design.at("schemaVersion")==1&&design.at("routeMeters").get<float>()<180,"Authored route has a bounded, measured scale");
    require(design.at("passageWidth").get<float>()>=3,"Passage provides character and camera clearance");
    unsigned textured=0;
    for(const auto& reference:design.at("environmentAssets")){
        const auto model=loadGltfAsset(assets.resolveReference(reference.get<std::string>()).u8string());
        require(!model.vertices.empty()&&!model.indices.empty(),"Blender exports load through the production glTF reader");
        const auto name=std::filesystem::path(reference.get<std::string>()).stem().string();
        if(name=="ground"||name=="trail"||name=="branch"){
            std::array<float,3> low{1e9F,1e9F,1e9F},high{-1e9F,-1e9F,-1e9F};
            for(const auto& vertex:model.vertices)for(unsigned axis=0;axis<3;++axis){low[axis]=std::min(low[axis],vertex.position[axis]);high[axis]=std::max(high[axis],vertex.position[axis]);}
            const auto height=high[1]-low[1],length=high[2]-low[2];
            require(height<=.51F&&length>=1.9F,"Walkable rendered meshes are horizontal Y-up surfaces matching collision geometry");
            if(name=="ground")require(std::abs(length-116)<.01F,"Rendered ground spans the entire exploration route");
        }
        for(const auto& material:model.materials)if(!material.baseColorPixels.empty()){
            ++textured;require(!material.normalPixels.empty()&&!material.metallicRoughnessPixels.empty(),"Environment PBR textures include normal and roughness data");
            require(material.textureSettings[0].colorSpace=="srgb"&&material.textureSettings[1].colorSpace=="linear"&&material.textureSettings[2].colorSpace=="linear","Texture color interpretation follows PBR semantics");
        }
    }
    require(textured>=4,"Ground, rock, vegetation and architecture have authored PBR surfaces");
    std::cout<<"Exploration scale, Blender mesh loading and PBR interpretation passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
