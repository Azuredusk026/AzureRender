#include "assets/MaterialData.hpp"
#include "assets/MaterialGraph.hpp"
#include "assets/PrimitiveGenerator.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
int main(){try{
    auto require=[](bool condition,const char* message){if(!condition)throw std::runtime_error(message);};
    const nlohmann::json complete={{"schemaVersion",3},{"typeId","azure.profile"},{"parameters",nlohmann::json::object()},
        {"textures",nlohmann::json::object()},{"surface",{{"asset",{{"version","2.0"}}},{"materials",nlohmann::json::array({{
            {"name","complete-fabric"},{"alphaMode","MASK"},{"alphaCutoff",.37},{"doubleSided",true},
            {"extras",{{"azureRenderMaterial",{{"schemaVersion",2},{"class","fabric"},{"features",{"silhouette-outline"}}}}}}}})}}}};
    const auto completeDocument=MaterialData::parse(complete);
    require(completeDocument.encode()==complete,"Complete material profiles must round trip without semantic loss");
    bool settingsCountRejected=false;
    try{(void)loadEmbeddedGltfMaterials(complete["surface"].dump(),false,{nlohmann::json::object()});}
    catch(const std::exception& error){settingsCountRejected=std::string(error.what()).find("image table")!=std::string::npos;}
    require(settingsCountRejected,"Embedded material import settings must match the source image table");
    require(MaterialGraph::fromMaterial(completeDocument).compile().encode()==complete,"Material graphs must preserve complete surface documents");
    const auto completeMaterial=completeDocument.interpret([](const auto&){return std::filesystem::path();});
    require(completeMaterial.materialClass==AssetMaterialClass::Fabric&&completeMaterial.doubleSided
        &&completeMaterial.alphaMode==AssetAlphaMode::Mask&&std::abs(completeMaterial.alphaCutoff-.37F)<.00001F
        &&(completeMaterial.materialFeatures&MaterialFeatureSilhouetteOutline),"Complete documents must consume profile and render state");
    auto invalidComplete=complete;invalidComplete["surface"]["buffers"]=nlohmann::json::array();
    bool geometryRejected=false;try{(void)MaterialData::parse(invalidComplete);}catch(const std::exception&){geometryRejected=true;}
    require(geometryRejected,"Material documents must reject geometry and external resource containers");
    auto incompatibleComplete=complete;
    incompatibleComplete["surface"]["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"hair-projected-ramp"};
    bool ownerRejected=false;try{(void)MaterialData::parse(incompatibleComplete).interpret([](const auto&){return std::filesystem::path();});}catch(const std::exception&){ownerRejected=true;}
    require(ownerRejected,"Complete documents must enforce the shared profile owner contract");
    incompatibleComplete=complete;incompatibleComplete["surface"]["materials"][0]["extras"]["azureRenderMaterial"]["typo"]=1;
    bool fieldRejected=false;try{(void)MaterialData::parse(incompatibleComplete).interpret([](const auto&){return std::filesystem::path();});}catch(const std::exception&){fieldRejected=true;}
    require(fieldRejected,"Complete documents must reject unknown profile fields");
    auto outlined=complete;
    outlined["surface"]["materials"][0]["extras"]["azureRenderMaterial"]["outlineSurface"]={
        {"schemaVersion",1},{"mode","view-scaled"},{"width",.7},{"minimumPixels",.4},
        {"depthOffset",.12},{"depthPrepass",true},{"color",{.1,.2,.3}}};
    const auto outlineDocument=MaterialData::parse(outlined);
    require(outlineDocument.encode()==outlined&&MaterialGraph::fromMaterial(outlineDocument).compile().encode()==outlined,
        "Complete documents and graphs must preserve the exact outline contract");
    const auto outlineMaterial=outlineDocument.interpret([](const auto&){return std::filesystem::path();});
    require(outlineMaterial.outlineSurface.present&&outlineMaterial.outlineSurface.depthPrepass
        &&std::abs(outlineMaterial.outlineSurface.width-.7F)<1e-6F&&outlineMaterial.outlineSurface.color[1]==.2F,
        "Complete material interpretation must consume depth, color and projection inputs");
    auto pbr=MaterialData::defaults("azure.pbr");pbr.parameters["baseColor"]={.5,.25,.125,1};pbr.parameters["roughness"]=.3;pbr.parameters["metallic"]=.8;
    const auto material=MaterialData::parse(pbr.encode()).interpret([](const auto&)->std::filesystem::path{throw std::runtime_error("No texture expected");});
    require(material.baseColorPixels[0]>material.baseColorPixels[1]&&material.metallicRoughnessPixels[1]==77&&material.metallicRoughnessPixels[2]==204,"Runtime interpretation must consume actual linear color and PBR parameters");
    auto style=MaterialData::defaults("azure.stylized");style.parameters["toonAmount"]=.5;
    const auto rendered=style.interpret([](const auto&){return std::filesystem::path();});
    require(rendered.materialFeatures==MaterialFeatureStylizedShadow&&rendered.styleParameters[0]==.5F,"Registered style parameters must change runtime material interpretation");
    for(const auto* type:{"azure.pbr","azure.stylized"}) {
        auto emission=MaterialData::defaults(type);
        emission.parameters["emissiveStrength"]=2.5;
        const auto lit=MaterialData::parse(emission.encode()).interpret([](const auto&){return std::filesystem::path();});
        require(lit.emissiveStrength==2.5F&&(lit.materialFeatures&MaterialFeatureEmissiveMask)!=0,
            "Saved emissive strength must enable the shader emission consumer");
        require((lit.materialFeatures&MaterialFeatureStylizedShadow)==(type==std::string("azure.stylized")?MaterialFeatureStylizedShadow:0),
            "Emission must preserve registered material features");
        emission.parameters["emissiveStrength"]=0;
        const auto dark=MaterialData::parse(emission.encode()).interpret([](const auto&){return std::filesystem::path();});
        require((dark.materialFeatures&MaterialFeatureEmissiveMask)==0,
            "Zero emission must preserve the default feature contract");
    }
    for(const auto& invalid:std::vector<nlohmann::json>{nlohmann::json{{"schemaVersion",2}},[&]{auto d=pbr.encode();d["parameters"]["roughness"]=-1;return d;}(),[&]{auto d=pbr.encode();d["textures"]["albedo"]={{"asset","assets:/bad.png"},{"usage","albedo"}};return d;}()}){bool rejected=false;try{MaterialData::parse(invalid);}catch(const std::exception&){rejected=true;}require(rejected,"Malformed material documents must fail deterministically");}
    const auto cubePath=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets_public/scene_inspector/assets/cube.gltf";
    nlohmann::json cube;{std::ifstream input(cubePath);input>>cube;}
    const auto metadataPath=std::filesystem::temp_directory_path()/("azure-slot-metadata-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".gltf");
    const std::vector<nlohmann::json> identityCases={nlohmann::json::array({{{"name","A"}},{{"name","B"}}}),
        nlohmann::json::array({nlohmann::json::object()}),
        nlohmann::json::array({nlohmann::json::object(),nlohmann::json::object()}),
        nlohmann::json::array({{{"name","same"}},{{"name","same"}}}),
        nlohmann::json::array({{{"name","FallbackMaterial"}}}),
        nlohmann::json::array({{{"name","named"},{"extras",{{"azureSlotId","authored"}}}}}),nlohmann::json::array()};
    for(const auto& materials:identityCases){
        auto document=cube;document["materials"]=materials;
        if(materials.empty())for(auto& mesh:document["meshes"])for(auto& primitive:mesh["primitives"])primitive.erase("material");
        {std::ofstream output(metadataPath);output<<document;}
        require(inspectMaterialSlots(metadataPath)==materialSlotIds(loadGltfAsset(metadataPath.u8string())),
            "Metadata identities must match complete model loading, including implicit fallback");
    }
    auto absentImage=cube;absentImage["images"]={{{"uri","missing-slot-inspection-image.png"}}};absentImage["textures"]={{{"source",0}}};
    absentImage["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]={{"index",0}};
    {std::ofstream output(metadataPath);output<<absentImage;}
    require(!inspectMaterialSlots(metadataPath).empty(),"Slot inspection must not require image decoding");
    bool fullImageRejected=false;try{static_cast<void>(loadGltfAsset(metadataPath.u8string()));}catch(const std::exception&){fullImageRejected=true;}
    require(fullImageRejected,"Full model preparation must retain missing-image validation");
    auto conflicting=cube;conflicting["materials"]={{{"extras",{{"azureSlotId","same"}}}},{{"extras",{{"azureSlotId","same"}}}}};
    {std::ofstream output(metadataPath);output<<conflicting;}
    bool metadataRejected=false;try{static_cast<void>(inspectMaterialSlots(metadataPath));}catch(const std::exception&){metadataRejected=true;}
    require(metadataRejected,"Metadata inspection must reject conflicting explicit identities");
    const auto binaryPath=std::filesystem::path(metadataPath.u8string()+".glb");auto encoded=cube.dump();encoded.append((4-encoded.size()%4)%4,' ');
    const auto writeWord=[](std::ofstream& output,std::uint32_t value){for(unsigned byte=0;byte<4;++byte)output.put(static_cast<char>((value>>(byte*8U))&255U));};
    {std::ofstream output(binaryPath,std::ios::binary);writeWord(output,0x46546c67U);writeWord(output,2);writeWord(output,static_cast<std::uint32_t>(20+encoded.size()));writeWord(output,static_cast<std::uint32_t>(encoded.size()));writeWord(output,0x4e4f534aU);output<<encoded;}
    require(inspectMaterialSlots(binaryPath)==materialSlotIds(loadGltfAsset(binaryPath.u8string())),"GLB inspection must preserve full-loader identities");
    {std::ofstream output(binaryPath,std::ios::binary);output<<"glTF";}
    metadataRejected=false;try{static_cast<void>(inspectMaterialSlots(binaryPath));}catch(const std::exception&){metadataRejected=true;}
    require(metadataRejected,"Truncated GLB headers must fail before material identity publication");
    for(const auto& header:std::vector<std::array<std::uint32_t,5>>{
            {0x46546c67U,3U,static_cast<std::uint32_t>(20+encoded.size()),static_cast<std::uint32_t>(encoded.size()),0x4e4f534aU},
            {0x46546c67U,2U,static_cast<std::uint32_t>(20+encoded.size()),static_cast<std::uint32_t>(encoded.size()),0x004e4942U},
            {0x46546c67U,2U,static_cast<std::uint32_t>(24+encoded.size()),static_cast<std::uint32_t>(encoded.size()+4),0x4e4f534aU}}){
        {std::ofstream output(binaryPath,std::ios::binary);for(const auto word:header)writeWord(output,word);output<<encoded;}
        metadataRejected=false;try{static_cast<void>(inspectMaterialSlots(binaryPath));}catch(const std::exception&){metadataRejected=true;}
        require(metadataRejected,"Invalid GLB versions, chunk types and declared lengths must be rejected");
    }
    for(const auto& invalidIdentity:std::vector<nlohmann::json>{"",42}){
        auto document=cube;document["materials"][0]["extras"]["azureSlotId"]=invalidIdentity;
        {std::ofstream output(metadataPath);output<<document;}
        metadataRejected=false;try{static_cast<void>(inspectMaterialSlots(metadataPath));}catch(const std::exception&){metadataRejected=true;}
        require(metadataRejected,"Malformed explicit source identities must retain strict rejection");
    }
    std::filesystem::remove(metadataPath);std::filesystem::remove(binaryPath);
    LoadedAsset mesh;AssetMaterial a,b;a.name="A";b.name="B";mesh.materials={a,b};const auto slots=materialSlotIds(mesh);std::swap(mesh.materials[0],mesh.materials[1]);require(materialSlotIds(mesh)[1]==slots[0],"Named source slots must survive material table reordering");
    a.slotId="stable";b.slotId="stable";mesh.materials={a,b};bool duplicate=false;try{materialSlotIds(mesh);}catch(const std::exception&){duplicate=true;}require(duplicate,"Conflicting explicit slot IDs must be rejected");
    const auto replacementPath=std::filesystem::temp_directory_path()/("azure-replacement-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".azurematerial");
    {std::ofstream output(replacementPath);output<<MaterialData::defaults("azure.pbr").encode();}
    AssetMaterial source;source.name="face-slot";source.slotId="source-slot";source.materialClass=AssetMaterialClass::Face;
    source.faceSdf.present=true;source.nprDiffuse.present=true;source.outlineSurface.present=true;
    source.outlineSurface.depthPrepass=true;source.outlineSurface.width=.65F;
    source.materialFeatures=MaterialFeatureDiffuseRamp|MaterialFeatureGeometryShadow;
    source.nprDiffuse.lighting={true,{1.2F,.8F,.6F},{.7F,.9F,1.1F}};
    source.nprDiffuse.rimResponse=AssetMaterial::NprDiffuse::RimResponse::ViewFacing;
    source.nprDiffuse.viewTint={true,.595F,{.38F,.12F,.14F}};
    source.nprDiffuse.mainRamp={true,.25F};
    source.nprDiffuse.shadowMask=AssetMaterial::NprDiffuse::ShadowMask::One;
    source.nprDiffuse.characterVisibility=AssetMaterial::NprDiffuse::CharacterVisibility::MainLightShadow;
    source.nprDiffuse.localDiffuseFloorScale=2;
    source.nprDiffuse.rimOrigin={true,10,"independent-body-origin"};
    mesh.materials={source};MaterialBinding binding;binding.path=replacementPath.u8string();
    auto& messages=RuntimeDiagnostics::instance().messages();const auto firstMessage=messages.size();
    applyMaterialBindings(mesh,{{"source-slot",binding}});
    require(messages.size()>firstMessage,"Full replacement must diagnose lost character capabilities");
    const auto& warning=messages.back();
    require(warning.find("source-slot")!=std::string::npos&&warning.find("faceSdf")!=std::string::npos
        &&warning.find("nprDiffuse")!=std::string::npos&&warning.find("diffuse-ramp")!=std::string::npos
        &&warning.find("outlineSurface")!=std::string::npos,
        "Replacement diagnostic must identify the slot and discarded semantics");
    require(mesh.materials[0].slotId==source.slotId&&mesh.materials[0].name==source.name&&!mesh.materials[0].faceSdf.present,
        "Diagnostics must preserve explicit full replacement and stable slot identity");
    const auto noLossMessages=messages.size();
    applyMaterialBindings(mesh,{{"source-slot",binding}});
    std::filesystem::remove(replacementPath);
    require(messages.size()==noLossMessages,"Equivalent ordinary material replacement must remain quiet");
    const nlohmann::json patch={{"schemaVersion",2},{"mode","parameters"},{"typeId","azure.pbr"},
        {"parameters",{{"emissiveStrength",3.0}}},{"textures",nlohmann::json::object()}};
    const auto patchDocument=MaterialData::parse(patch);
    require(patchDocument.encode()==patch,"Sparse parameter overrides must round trip without adding defaults");
    {std::ofstream output(replacementPath);output<<patchDocument.encode();}
    source.emissiveStrength=.5F;source.baseColorPixels={71,82,93,255};source.baseColorWidth=source.baseColorHeight=1;
    source.faceSdf.headNode=7;source.faceSdf.headNodeName="Head";source.faceSdf.pixels={9,8,7,6};
    source.specularRamp={true,true,.35F};
    mesh.materials={source};const auto patchMessages=messages.size();applyMaterialBindings(mesh,{{"source-slot",binding}});
    std::filesystem::remove(replacementPath);
    const auto& patched=mesh.materials[0];
    require(patched.emissiveStrength==3&&(patched.materialFeatures&MaterialFeatureEmissiveMask),"Override must reach its runtime consumer");
    require(patched.faceSdf.present&&patched.faceSdf.headNode==7&&patched.faceSdf.pixels==source.faceSdf.pixels
        &&patched.outlineSurface.present&&patched.outlineSurface.depthPrepass&&patched.outlineSurface.width==.65F
        &&patched.nprDiffuse.present&&patched.nprDiffuse.lighting.present
        &&patched.nprDiffuse.lighting.ambientColor==source.nprDiffuse.lighting.ambientColor
        &&patched.nprDiffuse.lighting.mainColor==source.nprDiffuse.lighting.mainColor
        &&patched.nprDiffuse.rimResponse==source.nprDiffuse.rimResponse
        &&patched.nprDiffuse.viewTint.present&&patched.nprDiffuse.viewTint.scale==source.nprDiffuse.viewTint.scale
        &&patched.nprDiffuse.viewTint.color==source.nprDiffuse.viewTint.color
        &&patched.nprDiffuse.mainRamp.present&&patched.nprDiffuse.mainRamp.bias==source.nprDiffuse.mainRamp.bias
        &&patched.nprDiffuse.shadowMask==source.nprDiffuse.shadowMask
        &&patched.nprDiffuse.characterVisibility==source.nprDiffuse.characterVisibility
        &&patched.nprDiffuse.localDiffuseFloorScale==source.nprDiffuse.localDiffuseFloorScale
        &&patched.nprDiffuse.rimOrigin.present&&patched.nprDiffuse.rimOrigin.node==source.nprDiffuse.rimOrigin.node
        &&patched.nprDiffuse.rimOrigin.nodeName==source.nprDiffuse.rimOrigin.nodeName
        &&patched.specularRamp.present&&patched.specularRamp.flipV
        &&patched.specularRamp.iridescence==source.specularRamp.iridescence
        &&(patched.materialFeatures&MaterialFeatureGeometryShadow)!=0
        &&patched.materialClass==source.materialClass&&patched.baseColorPixels==source.baseColorPixels,
        "Parameter override must retain unselected source fields and character inputs");
    require(messages.size()==patchMessages,"Preserving source semantics must not report replacement loss");
    auto sidedPatch=patch;sidedPatch["parameters"]={{"doubleSided",false}};source.doubleSided=true;
    {std::ofstream output(replacementPath);output<<sidedPatch;}
    mesh.materials={source};applyMaterialBindings(mesh,{{"source-slot",binding}});std::filesystem::remove(replacementPath);
    require(!mesh.materials[0].doubleSided&&messages.size()==patchMessages,"Explicit parameter edits must not report full replacement loss");
    auto physicalPatch=patch;physicalPatch["parameters"]={{"transmission",.5}};
    source.materialClass=AssetMaterialClass::Generic;source.specularInput.present=true;
    bool incompatible=false;try{MaterialData::parse(physicalPatch).interpret([](const auto&){return std::filesystem::path();},&source);}catch(const std::exception&){incompatible=true;}
    require(incompatible,"Parameter overrides must reject incompatible inherited surface owners");
    for(const auto& invalidPatch:std::vector<nlohmann::json>{[&]{auto x=patch;x["mode"]="merge";return x;}(),
        [&]{auto x=patch;x["parameters"]["typo"]=1;return x;}(),[&]{auto x=patch;x["parameters"]["emissiveStrength"]=nullptr;return x;}()}){
        bool rejected=false;try{MaterialData::parse(invalidPatch);}catch(const std::exception&){rejected=true;}
        require(rejected,"Override mode, fields and values must be strictly validated");
    }
    const auto instructions=std::filesystem::path(__FILE__).parent_path().parent_path()/"docs/runtime/material-authoring.md";
    std::ifstream documentation(instructions);require(bool(documentation),"Material authoring reference must be readable");
    std::string line,example;bool collecting=false;unsigned examples=0;
    while(std::getline(documentation,line)){
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        if(line=="```json"){collecting=true;example.clear();}
        else if(collecting&&line=="```"){static_cast<void>(MaterialData::parse(nlohmann::json::parse(example)));++examples;collecting=false;}
        else if(collecting)example+=line+'\n';
    }
    require(examples>=2&&!collecting,"Material examples must cover replacement and parameter inheritance");
    std::cout<<"Material round trip, physical parameters, registered style interpretation and stable slot identities passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
