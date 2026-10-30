#include "assets/AssetTransfer.hpp"
#include "assets/image/ImageCodec.hpp"
#include "assets/MaterialData.hpp"
#include "assets/image/TextureContainerAdapter.hpp"
#include "rhi/NullRhi.hpp"
#include "scenes/CharacterSceneRenderer.hpp"
#include "render/RenderContext.hpp"
#include "render/RenderSettings.hpp"
#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#include <ktx.h>
#ifdef _WIN32
#include <crtdbg.h>
#include <cstdlib>
#endif
using namespace azurerender;
namespace {
void require(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<class F>void rejects(F action,const char* reason){bool failed=false;try{action();}catch(const std::exception&){failed=true;}require(failed,reason);}
nlohmann::json apply(EditorSession& session,const std::string& operation,nlohmann::json args=nlohmann::json::object()){auto result=session.edit(operation,std::move(args));if(!result)throw std::runtime_error(operation+": "+result.diagnostics.dump());return result.value;}
nlohmann::json wait(EditorSession& session,const std::string& identity){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);while(std::chrono::steady_clock::now()<deadline){session.pollTasks();for(const auto& row:session.tasks().report())if(row.at("id")==identity&&row.at("state")!="running")return row;std::this_thread::sleep_for(std::chrono::milliseconds(1));}throw std::runtime_error("Texture exchange task exceeded deadline");}
void word(std::vector<unsigned char>& bytes,unsigned value){for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<unsigned char>(value>>(8*i)));}
std::string base64(const std::vector<unsigned char>& bytes){const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";std::string result;for(std::size_t i=0;i<bytes.size();i+=3){const unsigned value=(unsigned(bytes[i])<<16)|(i+1<bytes.size()?unsigned(bytes[i+1])<<8:0)|(i+2<bytes.size()?bytes[i+2]:0);result+=alphabet[(value>>18)&63];result+=alphabet[(value>>12)&63];result+=i+1<bytes.size()?alphabet[(value>>6)&63]:'=';result+=i+2<bytes.size()?alphabet[value&63]:'=';}return result;}
std::vector<unsigned char> dds(){
    std::vector<unsigned char> b{'D','D','S',' '};
    for(unsigned v:{124U,0x2100fU,2U,4U,16U,0U,3U})word(b,v);
    for(unsigned i=0;i<11;++i)word(b,0);
    for(unsigned v:{32U,0x41U,0U,32U,255U,65280U,16711680U,4278190080U,0x401008U,0U,0U,0U,0U})word(b,v);
    for(unsigned i=0;i<8;++i)b.insert(b.end(),{255,0,0,128});
    for(unsigned i=0;i<2;++i)b.insert(b.end(),{0,255,0,255});
    b.insert(b.end(),{0,0,255,255});return b;
}
}
int main(int argc,char** argv){
#ifdef _WIN32
    _set_abort_behavior(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);_CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
auto root=argc==4?std::filesystem::absolute(std::filesystem::u8path(argv[3])):std::filesystem::temp_directory_path()/("azure-containers-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));int result=0;try{
    require(!std::filesystem::exists(root),"Texture evidence output must be new");
    const auto& registry=assetFormatRegistry();
    for(const char* id:{"dds","ktx2"})require(registry.require(id).canRead&&registry.require(id).canWrite,"Containers need production import and export");
    const auto raw=dds();const auto image=decodeImageBytes(raw);
    require(image.width==4&&image.height==2&&image.rgba8.size()==32,"DDS base extent");
    require(image.rgba8[0]==255&&image.rgba8[3]==128,"DDS channel masks and alpha");
    require(image.mipLevels.size()==3&&image.mipLevels[1].width==2&&image.mipLevels[1].height==1&&image.mipLevels[1].rgba8==std::vector<unsigned char>({0,255,0,255,0,255,0,255})&&image.mipLevels[2].rgba8==std::vector<unsigned char>({0,0,255,255}),"Authored mip pixels must remain distinct from generated averages");
    std::filesystem::create_directories(root);const auto source=root/"authored.dds";
    {std::ofstream out(source,std::ios::binary);out.write(reinterpret_cast<const char*>(raw.data()),raw.size());}
    auto material=MaterialData::defaults("azure.pbr");material.parameters["baseColor"]={1,.5,1,1};material.textures["albedo"]={{"asset","00000000-0000-0000-0000-000000000001"},{"usage","albedo"}};
    const auto interpreted=material.interpret([&](const std::string&){return source;});
    require(interpreted.baseColorPixels.size()==44,"Material interpretation preserves the authored mip chain");
    require(interpreted.baseColorPixels[33]==188&&interpreted.baseColorPixels[42]==255,"Material factors apply consistently to all authored mip levels");
    require(argc==3||argc==4,"Public glTF fixture and shader paths are required");
    nlohmann::json document;{std::ifstream input(std::filesystem::u8path(argv[1]));input>>document;}
    document["images"][0]["uri"]="authored.dds";document["images"][0].erase("mimeType");
    document["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"]={1,1,1,1};
    document["samplers"][0]["minFilter"]=9987;
    std::ofstream(root/"authored.gltf")<<document.dump();const auto model=loadGltfAsset((root/"authored.gltf").u8string());
    require(model.materials.front().baseColorPixels.size()==44&&model.materials.front().baseColorPixels[33]==255,"Public glTF loading preserves authored mip samples");
    require(model.materials.front().authoredMipLevels[0]==3&&model.materials.front().textureSettings[0].mipPolicy=="generate","Authored mip upload fixture must enable mip filtering");
    auto embedded=document;embedded["images"][0]["uri"]="data:application/octet-stream;base64,"+base64(raw);const auto inlineAsset=loadEmbeddedGltfAsset(embedded.dump(),false);
    require(inlineAsset.materials.front().baseColorPixels.size()==44,"Embedded model candidates preserve authored texture mip levels");
    {
        rhi::NullRhi backend;backend.collectImageUploads=true;CharacterSceneRenderer renderer;RenderContext context{};context.rhi=&backend;context.allocator=&backend.allocator();context.shaderDirectory=argv[2];
        context.rampAtlasPath=(std::filesystem::u8path(argv[1]).parent_path()/"toon_ramp_atlas.ppm").u8string();context.descriptorLimits.maxPerStageDescriptorSamplers=4096;context.descriptorLimits.maxDescriptorSetSamplers=4096;context.descriptorLimits.maxPerStageDescriptorSampledImages=4096;context.descriptorLimits.maxDescriptorSetSampledImages=4096;context.descriptorLimits.maxPerStageResources=4096;
        context.scene.resources.push_back({"asset",(root/"authored.gltf").u8string()});scene::SceneNodeDesc node;node.id="model";node.resourceId="asset";context.scene.nodes.push_back(node);RenderSettings settings;context.renderSettings=&settings;
        const auto checkUpload=[&](unsigned levels,std::size_t bytes) {
            bool found=false;
            for(const auto& upload:backend.imageUploads)if(upload.width==4&&upload.height==2) {
                found=true;
                require(upload.levels==levels&&upload.pixels.size()==bytes,"Production upload respects the selected mip policy");
                if(levels==3)require(upload.pixels[33]==255&&upload.pixels[42]==255,"Production renderer uploads authored mip samples without regeneration");
            }
            require(found,"Authored texture reaches production renderer upload");
        };
        renderer.onLoad(context);checkUpload(3,44);renderer.onUnload(context);
        auto baseOnly=document;baseOnly["samplers"][0]["minFilter"]=9729;
        std::ofstream(root/"authored.gltf")<<baseOnly.dump();backend.imageUploads.clear();
        renderer.onLoad(context);checkUpload(1,32);renderer.onUnload(context);
        std::ofstream(root/"authored.gltf")<<document.dump();
        std::ofstream(root/"authored.dds.azmeta")<<nlohmann::json{{"settings",{{"texture",{{"mipPolicy","none"}}}}}}.dump();backend.imageUploads.clear();
        renderer.onLoad(context);checkUpload(1,32);renderer.onUnload(context);
        backend.unavailableSampledFormats.insert(VK_FORMAT_R8G8B8A8_SRGB);backend.unavailableSampledFormats.insert(VK_FORMAT_R8G8B8A8_UNORM);
        rejects([&]{try{renderer.onLoad(context);}catch(...){renderer.onUnload(context);throw;}renderer.onUnload(context);},"Device sampling capability rejection precedes unsupported upload");
    }
    for(const char* id:{"dds","ktx2"}){
        AssetTransferRequest request;request.operation=AssetTransferOperation::Export;request.formatId=id;request.source=source;request.staging=root/(std::string(id)+"-staging");request.destination=root/(std::string(id)+"-output");
        auto report=prepareAssetTransfer(request,registry,{});const auto exported=publishAssetTransfer(report);
        const auto recovered=decodeImageFile(exported);require(recovered.rgba8==image.rgba8,"Lossless containers preserve base pixels");
        require(recovered.mipLevels.size()==3&&recovered.mipLevels[1].rgba8==image.mipLevels[1].rgba8&&recovered.mipLevels[2].rgba8==image.mipLevels[2].rgba8,"All authored mip samples survive container exchange");
        require(report.summary.at("mipLevels")==3,"Container exchange preserves authored mip count");
        rejects([&]{prepareAssetTransfer(request,registry,[]{return true;});},"Cancelled container export is rejected");
    }
    AssetTransferRequest flat;flat.operation=AssetTransferOperation::Export;flat.formatId="png";flat.source=source;flat.staging=root/"flat-stage";flat.destination=root/"flat-output";
    require(!registry.require("png").plan(flat,{}).losses.empty(),"Flattening authored mips is disclosed before export");
    auto flattened=prepareAssetTransfer(flat,registry,{});require(!flattened.losses.empty(),"Flattening loss remains in the actual export report");discardAssetTransfer(flattened);
    auto colored=image;colored.colorSpace=ImageColorSpace::Srgb;
    auto midtone=colored;
    for(auto& level:midtone.mipLevels)for(std::size_t i=0;i<level.rgba8.size();i+=4)
        for(unsigned channel=0;channel<4;++channel)level.rgba8[i+channel]=std::array<unsigned char,4>{128,64,32,200}[channel];
    midtone.rgba8=midtone.mipLevels.front().rgba8;
    const auto encodedMidtone=encodeTextureContainer(midtone,"dds",{{"compression","bc7"}});
    {std::ofstream output(root/"midtone.dds",std::ios::binary);output.write(reinterpret_cast<const char*>(encodedMidtone.data()),encodedMidtone.size());}
    const auto decodedMidtone=decodeImageBytes(encodedMidtone);
    {std::ofstream output(root/"midtone-decoded.rgba",std::ios::binary);output.write(reinterpret_cast<const char*>(decodedMidtone.rgba8.data()),decodedMidtone.rgba8.size());}
    for(const auto& level:decodedMidtone.mipLevels)for(std::size_t i=0;i<level.rgba8.size();++i)
        require(std::abs(int(level.rgba8[i])-int(std::array<unsigned char,4>{128,64,32,200}[i%4]))<=5,
            "sRGB BC7 decoding must preserve stored midtone bytes across every mip");
    for(const std::string compression:{"bc3","bc7","basis"}){
        std::cerr<<"Checking "<<compression<<'\n';
        const auto encoded=encodeTextureContainer(colored,compression=="basis"?"ktx2":"dds",{{"compression",compression}});
        const auto fixture=root/(compression+(compression=="basis"?".ktx2":".dds"));std::ofstream bytes(fixture,std::ios::binary);bytes.write(reinterpret_cast<const char*>(encoded.data()),encoded.size());bytes.close();
        const auto decoded=decodeImageBytes(encoded);require(decoded.colorSpace==ImageColorSpace::Srgb&&decoded.mipLevels.size()==3,"Compressed color space and mip count survive decoding");
        for(std::size_t level=0;level<3;++level)for(std::size_t i=0;i<colored.mipLevels[level].rgba8.size();++i)require(std::abs(int(colored.mipLevels[level].rgba8[i])-int(decoded.mipLevels[level].rgba8[i]))<=24,"Uniform RGBA compression error exceeds 24 bytes");
        rejects([&]{decodeImageBytes(encoded,[]{return true;});},"Compressed decode cancellation");
    }
    for(const std::string compression:{"bc1","bc5"}){
        auto normal=image;normal.colorSpace=ImageColorSpace::Linear;for(auto& mip:normal.mipLevels)for(std::size_t i=0;i<mip.rgba8.size();i+=4){mip.rgba8[i]=128;mip.rgba8[i+1]=128;mip.rgba8[i+2]=255;mip.rgba8[i+3]=255;}normal.rgba8=normal.mipLevels.front().rgba8;
        const auto recovered=decodeImageBytes(encodeTextureContainer(normal,"dds",{{"compression",compression}}));require(recovered.colorSpace==ImageColorSpace::Linear,"Normal data remains linear");
        for(const auto& mip:recovered.mipLevels)for(std::size_t i=0;i<mip.rgba8.size();i+=4)require(std::abs(int(mip.rgba8[i])-128)<=8&&std::abs(int(mip.rgba8[i+1])-128)<=8,"Normal XY compression error exceeds 8 bytes");
        if(compression=="bc5"){
            const auto bytes=encodeTextureContainer(normal,"dds",{{"compression",compression}});const auto path=root/"normal.dds";std::ofstream output(path,std::ios::binary);output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());output.close();
            auto material=MaterialData::defaults("azure.pbr");material.textures["normal"]={{"asset","00000000-0000-0000-0000-000000000001"},{"usage","normal"}};const auto value=material.interpret([&](const std::string&){return path;});
            for(std::size_t i=0;i<value.normalPixels.size();i+=4)require(value.normalPixels[i+2]>=254&&value.normalPixels[i+3]==255,"BC5 normal usage reconstructs positive Z on every mip");
        }
    }
    auto cube=raw;cube[112]=0;cube[113]=2;rejects([&]{decodeImageBytes(cube);},"Cubemap support is reported separately");
    const auto ktx=encodeTextureContainer(image,"ktx2",nlohmann::json::object());
    unsigned checks=0;rejects([&]{encodeTextureContainer(image,"dds",{{"compression","bc7"}},[&]{return ++checks>3;});},"BC encoding cancellation is observed during codec work");
    auto headerOverlap=ktx;std::fill(headerOverlap.begin()+80,headerOverlap.begin()+88,static_cast<unsigned char>(0));rejects([&]{decodeImageBytes(headerOverlap);},"KTX2 level payload cannot overlap its header");
    auto expansion=ktx;for(unsigned i=0;i<8;++i)expansion[96+i]=static_cast<unsigned char>((257ULL*1024*1024)>>(8*i));rejects([&]{decodeImageBytes(expansion);},"KTX2 declared decompression budget is checked before library allocation");
    bool expansionBudget=false;try{decodeImageBytes(expansion);}catch(const std::exception& error){expansionBudget=std::string(error.what()).find("KTX2 declared mip storage exceeds 256 MiB")!=std::string::npos;}
    require(expansionBudget,"KTX2 declared mip storage is bounded before library allocation");
    DecodedImage uneven;uneven.width=7;uneven.height=3;uneven.colorSpace=ImageColorSpace::Linear;
    uneven.mipLevels={{7,3,std::vector<unsigned char>(84,23)},{3,1,std::vector<unsigned char>(12,61)},{1,1,{97,97,97,97}}};uneven.rgba8=uneven.mipLevels.front().rgba8;
    for(const char* format:{"dds","ktx2"}){const auto result=decodeImageBytes(encodeTextureContainer(uneven,format,nlohmann::json::object()));require(result.mipLevels.size()==3&&result.mipLevels[1].width==3&&result.mipLevels[1].height==1&&result.mipLevels[1].rgba8==std::vector<unsigned char>(12,61)&&result.mipLevels[2].rgba8==std::vector<unsigned char>({97,97,97,97}),"Non-square non-power-of-two authored mip dimensions and samples");}
    {
        DecodedImage directional;directional.width=directional.height=2;directional.rgba8={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};const auto bytes=encodeTextureContainer(directional,"ktx2",nlohmann::json::object());
        ktxTexture2* texture=nullptr;require(ktxTexture2_CreateFromMemory(bytes.data(),bytes.size(),KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,&texture)==KTX_SUCCESS,"Create upstream orientation fixture");
        require(ktxHashList_AddKVPair(&texture->kvDataHead,KTX_ORIENTATION_KEY,3,"lu")==KTX_SUCCESS,"Write orientation fixture metadata");ktx_uint8_t* data=nullptr;ktx_size_t count=0;require(ktxTexture_WriteToMemory(ktxTexture(texture),&data,&count)==KTX_SUCCESS,"Serialize orientation fixture");std::vector<unsigned char> encoded(data,data+count);std::free(data);ktxTexture_Destroy(ktxTexture(texture));
        const auto recovered=decodeImageBytes(encoded);require(recovered.rgba8==std::vector<unsigned char>({255,255,255,255,0,0,255,255,0,255,0,255,255,0,0,255}),"KTX2 orientation becomes top-left RGBA row order");
    }
    auto truncated=raw;truncated.pop_back();rejects([&]{decodeImageBytes(truncated);},"Truncated authored mip is rejected");
    auto huge=raw;huge[16]=1;huge[17]=32;rejects([&]{decodeImageBytes(huge);},"Dimensions above 8192 are rejected before allocation");
    {
        auto oversized=encodeTextureContainer(image,"dds",nlohmann::json::object());
        auto set=[&](std::size_t offset,unsigned value){for(unsigned i=0;i<4;++i)oversized[offset+i]=static_cast<unsigned char>(value>>(8*i));};
        set(12,4096);set(16,4096);set(28,1);set(128,2); // DXGI_FORMAT_R32G32B32A32_FLOAT: 256 MiB of source pixels.
        bool budgetRejected=false;try{decodeImageBytes(oversized);}catch(const std::exception& error){budgetRejected=std::string(error.what()).find("DDS source mip storage exceeds 64 MiB")!=std::string::npos;}
        require(budgetRejected,"DDS source storage budget precedes library allocation even when RGBA8 fits");
    }
    nlohmann::json workflows=nlohmann::json::array();
    for(const std::string name:{"dds","basis"}){
        const auto project=root/name;Project::create(project,name);nlohmann::json descriptor;{std::ifstream input(project/"project.azureproject");input>>descriptor;}descriptor["startupScene"]="assets:/creation.azurelevel";std::ofstream(project/"project.azureproject")<<descriptor.dump(2);
        std::ofstream(project/"assets/creation.azurelevel")<<nlohmann::json{{"schemaVersion",1},{"id",name},{"sceneType","sample"},{"resources",nlohmann::json::array()},{"nodes",nlohmann::json::array()}}.dump(2);
        auto context=EditorContext::openProject(project/"project.azureproject");EditorSession session(context);
        const auto model=apply(session,"asset.import",{{"path",argv[1]}}).get<std::string>();
        const auto texture=apply(session,"asset.import",{{"path",(root/(name=="dds"?"bc7.dds":"basis.ktx2")).u8string()}}).get<std::string>();
        apply(session,"node.place",{{"resource",model},{"id","authored-model"}});
        const auto material=apply(session,"material.create",{{"type","azure.pbr"},{"destination","assets:/container.azurematerial"}}).at("asset").get<std::string>();
        apply(session,"material.field",{{"asset",material},{"field","baseColor"},{"value",{1,1,1,1}}});apply(session,"material.field",{{"asset",material},{"field","textures.albedo"},{"value",{{"asset",texture},{"usage","albedo"}}}});apply(session,"material.save",{{"asset",material}});
        const auto slot=apply(session,"material.slots",{{"node","authored-model"}}).at("slots").at(0).at("id");apply(session,"material.assign",{{"nodes",{"authored-model"}},{"slotId",slot},{"material",material}});apply(session,"document.save");
        for(const char* mip:{"none","generate"}){const auto fingerprint=apply(session,"asset.inspect",{{"asset",texture}}).at("fingerprint");const auto task=apply(session,"asset.reimport",{{"asset",texture},{"baseFingerprint",fingerprint},{"settings",{{"texture",{{"mipPolicy",mip}}}}}}).at("task").get<std::string>();require(wait(session,task).at("state")=="completed","Container reimport preserves authored resource identity");}
        apply(session,"document.save");auto reopened=EditorContext::openProject(project/"project.azureproject");const auto image=loadAssetImage(reopened->assets().resolve(texture).u8string());require(image.mipLevels.size()==3,"Public reimport preserves authored mip samples");
        const auto binding=apply(session,"material.slots",{{"node","authored-model"}});require(binding.at("slots").at(0).at("override")==material&&binding.at("diagnostics").empty(),"Container reimport preserves authored material slots");
        const auto format=name=="dds"?"ktx2":"dds",compression=name=="dds"?"basis":"bc7";
        const auto plan=apply(session,"asset.export-plan",{{"asset",texture},{"format",format},{"destination",(root/(name+"-exchange")).u8string()},{"settings",{{"compression",compression}}}});const auto task=apply(session,"asset.export",{{"plan",plan.at("plan")}}).at("task").get<std::string>();const auto output=wait(session,task);require(output.at("state")=="completed","Public compressed exchange completes");
        const auto exchanged=std::filesystem::u8path(output.at("result").at("output").get<std::string>());require(decodeImageFile(exchanged).mipLevels.size()==3,"Public compressed exchange preserves mip count");
        workflows.push_back({{"name",name},{"project",(project/"project.azureproject").u8string()},{"texture",texture},{"material",material},{"mipLevels",3},{"output",exchanged.u8string()}});
    }
    std::ofstream(root/"authoring-summary.json")<<nlohmann::json{{"passed",true},{"workflows",workflows},{"actualMipUpload",true},{"boundedFailures",true}}.dump(2);
    std::cout<<"DDS and KTX2 production transfer, authored mip preservation and bounded failures passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';result=1;}if(argc!=4)std::filesystem::remove_all(root);return result;}
