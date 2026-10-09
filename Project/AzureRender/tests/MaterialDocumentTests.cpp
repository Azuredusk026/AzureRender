#include "assets/MaterialData.hpp"
#include "assets/PrimitiveGenerator.hpp"
#include <iostream>
using namespace azurerender;
int main(){try{
    auto require=[](bool condition,const char* message){if(!condition)throw std::runtime_error(message);};
    auto pbr=MaterialData::defaults("azure.pbr");pbr.parameters["baseColor"]={.5,.25,.125,1};pbr.parameters["roughness"]=.3;pbr.parameters["metallic"]=.8;
    const auto material=MaterialData::parse(pbr.encode()).interpret([](const auto&)->std::filesystem::path{throw std::runtime_error("No texture expected");});
    require(material.baseColorPixels[0]>material.baseColorPixels[1]&&material.metallicRoughnessPixels[1]==77&&material.metallicRoughnessPixels[2]==204,"Runtime interpretation must consume actual linear color and PBR parameters");
    auto style=MaterialData::defaults("azure.stylized");style.parameters["toonAmount"]=.5;
    const auto rendered=style.interpret([](const auto&){return std::filesystem::path();});
    require(rendered.materialFeatures==MaterialFeatureStylizedShadow&&rendered.styleParameters[0]==.5F,"Registered style parameters must change runtime material interpretation");
    for(const auto& invalid:std::vector<nlohmann::json>{nlohmann::json{{"schemaVersion",2}},[&]{auto d=pbr.encode();d["parameters"]["roughness"]=-1;return d;}(),[&]{auto d=pbr.encode();d["textures"]["albedo"]={{"asset","assets:/bad.png"},{"usage","albedo"}};return d;}()}){bool rejected=false;try{MaterialData::parse(invalid);}catch(const std::exception&){rejected=true;}require(rejected,"Malformed material documents must fail deterministically");}
    LoadedAsset mesh;AssetMaterial a,b;a.name="A";b.name="B";mesh.materials={a,b};const auto slots=materialSlotIds(mesh);std::swap(mesh.materials[0],mesh.materials[1]);require(materialSlotIds(mesh)[1]==slots[0],"Named source slots must survive material table reordering");
    a.slotId="stable";b.slotId="stable";mesh.materials={a,b};bool duplicate=false;try{materialSlotIds(mesh);}catch(const std::exception&){duplicate=true;}require(duplicate,"Conflicting explicit slot IDs must be rejected");
    std::cout<<"Material round trip, physical parameters, registered style interpretation and stable slot identities passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
