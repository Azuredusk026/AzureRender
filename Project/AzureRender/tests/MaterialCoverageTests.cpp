#include "assets/GltfLoader.hpp"
#include "assets/MaterialData.hpp"
#include "assets/MaterialGraph.hpp"
#include "assets/MaterialSurfaceProfile.hpp"
#include "resources/ResourceLocator.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <stb_image_write.h>
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
using Json=nlohmann::json;
int main(){
    const auto root=std::filesystem::temp_directory_path()/("azure-coverage-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try{
        std::filesystem::create_directories(root);
        Json source;std::ifstream(ResourceLocator().publicAsset("test_model.gltf"))>>source;
        const unsigned char pixels[]={0,64,128,255,255,64,128,0};
        if(!stbi_write_png((root/"coverage.png").string().c_str(),2,1,4,pixels,8))throw std::runtime_error("Cannot write coverage fixture");
        const auto image=source["images"].size();source["images"].push_back({{"uri","coverage.png"}});
        const auto texture=source["textures"].size();source["textures"].push_back({{"source",image}});
        auto& mat=source["materials"][0];mat["alphaMode"]="MASK";mat["name"]="IndependentCoverage";
        mat["pbrMetallicRoughness"].erase("metallicRoughnessTexture");
        mat["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","generic"},{"features",Json::array()},
            {"coverageTexture",{{"schemaVersion",1},{"texture",texture}}}};
        const auto load=[&](const Json& value){std::ofstream(root/"test.gltf")<<value.dump();return loadGltfAsset((root/"test.gltf").string()).materials[0];};
        const auto require=[](bool yes,const char* text){if(!yes)throw std::runtime_error(text);};
        const auto red=load(source);
        require(red.metallicRoughnessWidth==2&&red.metallicRoughnessHeight==1
            &&red.metallicRoughnessPixels[3]==0&&red.metallicRoughnessPixels[7]==255,"Coverage must keep independent image size and copy red into alpha");
        require(red.coverageTexture.present&&(red.materialFeatures&MaterialFeatureIndependentCoverage),"Typed coverage and feature ownership must agree");
        require(red.metallicRoughnessPixels[0]==255&&red.metallicRoughnessPixels[4]==255,"Coverage RGB must keep neutral physical AO");
        auto defaultSource=source;defaultSource["materials"][0]["extras"]["azureRenderMaterial"].erase("coverageTexture");
        const auto defaultMaterial=load(defaultSource);
        require(!defaultMaterial.coverageTexture.present&&packMaterialSurfaceProfile(defaultMaterial).rows==packMaterialSurfaceProfile(red).rows,
            "Independent coverage must not alter the packed surface profile");
        require(defaultMaterial.baseColorPixels==red.baseColorPixels,"Coverage must preserve base RGBA independently");
        for(unsigned pixel=0;pixel<2;++pixel)for(unsigned channel=0;channel<3;++channel)
            require(red.metallicRoughnessPixels[pixel*4+channel]==defaultMaterial.metallicRoughnessPixels[channel],"Physical factors must equal the texture-free default");
        auto alpha=source;alpha["materials"][0]["extras"]["azureRenderMaterial"]["coverageTexture"]["channel"]="alpha";
        const auto alphaMaterial=load(alpha);
        require(alphaMaterial.metallicRoughnessPixels[3]==255&&alphaMaterial.metallicRoughnessPixels[7]==0,"Coverage alpha selection must preserve original values");
        for(const auto& invalid:Json::array({nullptr,true,Json::object(),{{"schemaVersion",1.0},{"texture",texture}},
            {{"schemaVersion",2},{"texture",texture}},{{"schemaVersion",1},{"texture",-1}},
            {{"schemaVersion",1},{"texture",texture+100}},{{"schemaVersion",1},{"texture",texture},{"channel","green"}},
            {{"schemaVersion",1},{"texture",texture},{"channel",false}},{{"schemaVersion",1},{"texture",texture},{"extra",0}}})){
            auto value=source;value["materials"][0]["extras"]["azureRenderMaterial"]["coverageTexture"]=invalid;
            bool rejected=false;try{(void)load(value);}catch(const std::exception& error){rejected=std::string(error.what()).find("coverageTexture")!=std::string::npos;}
            require(rejected,"Invalid coverage input must identify coverageTexture");
        }
        for(const auto* mode:{"OPAQUE","BLEND"}){
            auto value=source;value["materials"][0]["alphaMode"]=mode;bool rejected=false;
            try{(void)load(value);}catch(const std::exception&){rejected=true;}require(rejected,"Coverage requires MASK");
        }
        for(const auto* category:{"generic","fabric","hair"}){
            auto value=source;value["materials"][0]["extras"]["azureRenderMaterial"]["class"]=category;(void)load(value);
        }
        auto conflict=source;conflict["materials"][0]["pbrMetallicRoughness"]["metallicRoughnessTexture"]={{"index",texture}};
        bool rejected=false;try{(void)load(conflict);}catch(const std::exception&){rejected=true;}require(rejected,"Coverage and physical parameter images must reject shared slot ownership");
        for(const auto* category:{"skin","face","eye","metal","overlay"}){
            auto value=source;value["materials"][0]["extras"]["azureRenderMaterial"]["class"]=category;bool failed=false;
            try{(void)load(value);}catch(const std::exception&){failed=true;}require(failed,"Unsupported coverage owners must fail");
        }
        auto old=source;old["materials"][0]["extras"]["azureRenderMaterial"]["schemaVersion"]=1;
        rejected=false;try{(void)load(old);}catch(const std::exception&){rejected=true;}require(rejected,"Explicit coverage requires profile two");
        auto typed=red;typed.materialFeatures&=~MaterialFeatureIndependentCoverage;
        rejected=false;try{(void)packMaterialSurfaceProfile(typed);}catch(const std::exception&){rejected=true;}require(rejected,"Direct profile construction must validate feature ownership");
        typed=red;typed.specularInput.source=AssetMaterial::SpecularInput::Source::MetallicRoughnessAlpha;
        rejected=false;try{(void)packMaterialSurfaceProfile(typed);}catch(const std::exception&){rejected=true;}require(rejected,"Direct specular alpha ownership must reject coverage conflicts");
        typed=red;typed.nprDiffuse.baseAo=AssetMaterial::NprDiffuse::BaseAo::MetallicRoughnessRed;
        rejected=false;try{(void)packMaterialSurfaceProfile(typed);}catch(const std::exception&){rejected=true;}require(rejected,"NPR AO ownership must reject coverage conflicts");
        Json profileSurface={{"asset",{{"version","2.0"}}},{"materials",Json::array({source["materials"][0]})},
            {"textures",Json::array({{{"source",0}}})}};
        profileSurface["materials"][0]["pbrMetallicRoughness"].erase("baseColorTexture");
        profileSurface["materials"][0].erase("normalTexture");profileSurface["materials"][0].erase("occlusionTexture");profileSurface["materials"][0].erase("emissiveTexture");
        profileSurface["materials"][0]["extras"]["azureRenderMaterial"]["coverageTexture"]["texture"]=0;
        Json document={{"schemaVersion",3},{"typeId","azure.profile"},{"parameters",Json::object()},
            {"textures",{{"0",{{"asset","00000000-0000-4000-8000-000000000001"},{"usage","linear"}}}}},{"surface",profileSurface}};
        const auto data=MaterialData::parse(document);require(data.encode()==document&&MaterialGraph::fromMaterial(data).compile().encode()==document,"Complete documents and graph saving must preserve independent coverage");
        const auto interpreted=data.interpret([&](const auto&){return root/"coverage.png";});
        require(interpreted.coverageTexture.present&&interpreted.metallicRoughnessPixels[7]==255,"Complete profile interpretation must consume coverage pixels");
        auto scalar=MaterialData::parse({{"schemaVersion",2},{"mode","parameters"},{"typeId","azure.pbr"},{"parameters",{{"roughness",.5}}},{"textures",Json::object()}});
        const auto overridden=scalar.interpret([](const auto&){return std::filesystem::path();},&red);
        require(overridden.coverageTexture.present&&overridden.metallicRoughnessPixels[3]==0&&overridden.metallicRoughnessPixels[7]==255,"Scalar overrides must preserve coverage alpha");
        scalar.textures["metallicRoughness"]={{"asset","00000000-0000-4000-8000-000000000002"},{"usage","roughness"}};
        rejected=false;try{(void)scalar.interpret([&](const auto&){return root/"coverage.png";},&red);}catch(const std::exception& error){rejected=std::string(error.what()).find("coverageTexture")!=std::string::npos;}
        require(rejected,"Parameter texture overrides must enforce coverage ownership");
        // An authored 4x4/2x2/1x1 DDS chain has distinct coverage at each level.
        std::vector<unsigned char> dds(148,0);const auto word=[&](unsigned offset,unsigned value){for(unsigned byte=0;byte<4;++byte)dds[offset+byte]=static_cast<unsigned char>(value>>(byte*8));};
        word(0,0x20534444);word(4,124);word(8,0x2100f);word(12,4);word(16,4);word(20,16);word(28,3);
        word(76,32);word(80,4);word(84,0x30315844);word(108,0x401008);word(128,28);word(132,3);word(140,1);
        for(unsigned level=0;level<3;++level)for(unsigned pixel=0;pixel<(16U>>(level*2));++pixel){dds.push_back(static_cast<unsigned char>(50+level*80));dds.push_back(10);dds.push_back(20);dds.push_back(255);}
        {std::ofstream out(root/"coverage.dds",std::ios::binary);out.write(reinterpret_cast<const char*>(dds.data()),dds.size());}
        auto mipSource=source;mipSource["images"][image]["uri"]="coverage.dds";
        const auto mipMaterial=load(mipSource);
        require(mipMaterial.authoredMipLevels[2]==3&&mipMaterial.metallicRoughnessWidth==4&&mipMaterial.metallicRoughnessPixels.size()==84,"Coverage must keep authored dimensions and all levels");
        require(mipMaterial.metallicRoughnessPixels[3]==50&&mipMaterial.metallicRoughnessPixels[67]==130&&mipMaterial.metallicRoughnessPixels[83]==210,"Selected coverage channel must be converted at every authored level");
        std::ofstream(root/"coverage.dds.azmeta")<<Json{{"schemaVersion",1},{"id","00000000-0000-4000-8000-000000000001"},{"settings",{{"texture",{{"filter","linear"},{"mipFilter","nearest"},{"anisotropy",1},{"mipBias",-1}}}}}};
        const auto sampled=load(mipSource);require(sampled.textureSettings[2].mipBias==-1&&sampled.textureSettings[2].sampler(3,16).maxLod==2
            &&sampled.textureSettings[2].mipFilter==TextureImportSettings::MipFilter::Nearest,"Coverage must use its own sampler and mip range");
        auto plain=MaterialData::defaults("azure.pbr");std::ofstream(root/"replacement.azurematerial")<<plain.encode();
        LoadedAsset replaced;replaced.materials={red};replaced.materials[0].slotId="coverage-slot";
        MaterialBinding binding;binding.path=(root/"replacement.azurematerial").string();
        const auto firstMessage=RuntimeDiagnostics::instance().messages().size();applyMaterialBindings(replaced,{{"coverage-slot",binding}});
        const auto& messages=RuntimeDiagnostics::instance().messages();require(messages.size()>firstMessage&&messages.back().find("coverageTexture")!=std::string::npos,
            "Full replacement must name discarded coverage ownership");
        std::filesystem::remove_all(root);std::cout<<"Independent coverage image size, channels, version and ownership passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
