#include "editor/AssetImportJob.hpp"
#include "assets/GltfLoader.hpp"
#include "assets/GenerationManifest.hpp"
#include "assets/TextureImportSettings.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include <stb_image.h>
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
namespace azurerender {
AssetImportJob::AssetImportJob(std::filesystem::path source,std::filesystem::path staging,std::filesystem::path target):destination(std::move(target)),staging_(std::move(staging)),filename_(source.filename().string()){
    if(source.extension()!=".gltf"&&source.extension()!=".glb")throw std::invalid_argument("Import expects glTF or GLB");
    work_=std::async(std::launch::async,[this,source]{prepare(source);});
}
AssetImportJob::~AssetImportJob(){cancel();if(work_.valid())work_.wait();std::error_code error;std::filesystem::remove_all(staging_,error);}
void AssetImportJob::checkCancel() const{if(cancelled_)throw std::runtime_error("Asset import cancelled");}
void AssetImportJob::copy(const std::filesystem::path& source,const std::filesystem::path& target){
    checkCancel();std::filesystem::create_directories(target.parent_path());std::ifstream input(source,std::ios::binary);if(!input)throw std::runtime_error("Import dependency missing: "+source.string());std::ofstream output(target,std::ios::binary);
    std::array<char,65536> bytes{};while(input){checkCancel();input.read(bytes.data(),bytes.size());output.write(bytes.data(),input.gcount());}output.flush();if(!input.eof()||!output)throw std::runtime_error("Import copy failed");
}
void AssetImportJob::prepare(const std::filesystem::path& source){
    copy(source,staging_/source.filename());progress_=0.2F;
    if(source.extension()==".gltf"){
        std::ifstream input(source);nlohmann::json document;input>>document;
        for(const char* key:{"buffers","images"})for(const auto& entry:document.value(key,nlohmann::json::array())){
            const auto uri=entry.value("uri",std::string());if(uri.empty()||uri.rfind("data:",0)==0)continue;
            const auto relative=std::filesystem::path(uri).lexically_normal();if(relative.is_absolute()||relative.has_root_name()||*relative.begin()=="..")throw std::invalid_argument("Imported dependency must stay within its asset directory");
            copy(source.parent_path()/relative,staging_/relative);
        }
    }
    progress_=0.65F;checkCancel();const auto asset=loadGltfAsset((staging_/source.filename()).string());
    summary_={{"source",source.string()},{"vertices",asset.vertices.size()},{"indices",asset.indices.size()},
        {"materials",asset.materials.size()},{"joints",asset.jointNodes.size()},{"skinned",asset.hasSkin},{"clips",nlohmann::json::array()}};
    for(std::size_t index=0;index<asset.animations.size();++index){const auto& clip=asset.animations[index];
        summary_["clips"].push_back({{"index",index},{"name",clip.name},{"duration",clip.endTime-clip.startTime},{"channels",clip.channels.size()}});}
    GenerationManifest manifest;manifest.generatorId="azure.gltf-import";manifest.generatorVersion=1;
    manifest.parametersHash=generationHash(nlohmann::json{{"filename",source.filename().generic_string()},{"importerVersion",1}}.dump());
    manifest.licenseSource="LicenseRef-External-Asset";
    for(const auto& entry:std::filesystem::recursive_directory_iterator(staging_))if(entry.is_regular_file()) {
        checkCancel();std::ifstream input(entry.path(),std::ios::binary);const std::string content{std::istreambuf_iterator<char>(input),{}};
        const auto name=entry.path().lexically_relative(staging_).generic_string();
        manifest.inputHashes[name]=generationHash(content);manifest.outputHashes[name]=manifest.inputHashes[name];
    }
    summary_["generation"]=GenerationManifest::fromJson(manifest.toJson()).toJson();
    checkCancel();progress_=1;
}
bool AssetImportJob::ready()const{return work_.valid()&&work_.wait_for(std::chrono::seconds(0))==std::future_status::ready;}
std::filesystem::path AssetImportJob::finish(){work_.get();checkCancel();std::filesystem::create_directories(destination.parent_path());std::filesystem::rename(staging_,destination);return destination/filename_;}
AssetReimportJob::AssetReimportJob(AssetRecord asset,std::vector<AssetRecord> inputs,nlohmann::json settings) {
    work_=std::async(std::launch::async,[this,asset=std::move(asset),inputs=std::move(inputs),settings=std::move(settings)] {
        std::size_t count=0;
        for(const auto& input:inputs) {
            check();std::ifstream stream(input.path,std::ios::binary);
            if(!stream)throw std::runtime_error("Reimport source is unavailable");
            std::array<char,65536> chunk{};std::uint64_t hash=14695981039346656037ULL;
            while(stream){check();stream.read(chunk.data(),chunk.size());for(std::streamsize i=0;i<stream.gcount();++i){hash^=static_cast<unsigned char>(chunk[static_cast<std::size_t>(i)]);hash*=1099511628211ULL;}}
            if(!stream.eof()||hash!=input.contentHash)throw std::runtime_error("Reimport input changed during preparation");
            progress_=.5F*static_cast<float>(++count)/static_cast<float>(inputs.size());
        }
        check();
        const auto extension=asset.path.extension().u8string();
        if(extension==".gltf"||extension==".glb")static_cast<void>(loadGltfAsset(asset.path.u8string()));
        else if(assetTypeRegistry().matches(asset.path,{"image"})) {
            int width=0,height=0,channels=0;
            std::unique_ptr<unsigned char,void(*)(void*)> pixels(stbi_load(asset.path.u8string().c_str(),&width,&height,&channels,4),stbi_image_free);
            if(!pixels)throw std::runtime_error("Cannot decode reimported image");
            const auto texture=TextureImportSettings::parse(settings.value("texture",nlohmann::json::object()));
            if(texture.mipPolicy=="generate") {
                const auto size=static_cast<std::size_t>(width)*static_cast<std::size_t>(height)*4;
                static_cast<void>(buildTextureMipChain({pixels.get(),pixels.get()+size},static_cast<unsigned>(width),static_cast<unsigned>(height),texture.semantic()));
            }
        }
        check();progress_=1;
        return nlohmann::json{{"asset",asset.id},{"validatedInputs",inputs.size()},{"settings",settings}};
    });
}
AssetReimportJob::~AssetReimportJob(){cancel();if(work_.valid())work_.wait();}
void AssetReimportJob::check() const {if(cancelled_)throw std::runtime_error("Asset reimport cancelled");}
bool AssetReimportJob::ready() const{return work_.valid()&&work_.wait_for(std::chrono::seconds(0))==std::future_status::ready;}
nlohmann::json AssetReimportJob::finish(){auto result=work_.get();check();return result;}
}
