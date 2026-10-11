#include "editor/AssetImportJob.hpp"
#include "assets/GltfLoader.hpp"
#include "assets/GenerationManifest.hpp"
#include "assets/TextureImportSettings.hpp"
#include "assets/audio/AudioCodec.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include <stb_image.h>
#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
namespace azurerender {
AssetImportJob::AssetImportJob(std::filesystem::path source,std::filesystem::path staging,std::filesystem::path target):destination(std::move(target)),staging_(std::move(staging)),filename_(source.filename().u8string()) {
    const auto extension=assetExtension(source);
    bool supported=false;
    for(const auto& pair:assetFormatRegistry().formats()) {
        const auto& format=pair.second;
        supported=supported||(format.canRead&&std::find(format.extensions.begin(),format.extensions.end(),extension)!=format.extensions.end());
    }
    if(!supported)throw std::invalid_argument("Import expects a registered readable asset format");
    work_=std::async(std::launch::async,[this,source]{prepare(source);});
}
AssetImportJob::~AssetImportJob(){cancel();if(work_.valid())work_.wait();discardAssetTransfer(transfer_);}
void AssetImportJob::checkCancel() const{if(cancelled_)throw std::runtime_error("Asset import cancelled");}
void AssetImportJob::prepare(const std::filesystem::path& source) {
    AssetTransferRequest request;request.source=source;request.staging=staging_;request.destination=destination;
    progress_=0.2F;
    transfer_=prepareAssetTransfer(request,assetFormatRegistry(),[this]{return cancelled_.load();});
    summary_=transfer_.summary;checkCancel();progress_=1;
}
bool AssetImportJob::ready()const{return work_.valid()&&work_.wait_for(std::chrono::seconds(0))==std::future_status::ready;}
std::filesystem::path AssetImportJob::finish(){work_.get();checkCancel();return publishAssetTransfer(transfer_,[this]{return cancelled_.load();});}
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
        const auto extension=assetExtension(asset.path);
        if(extension==".gltf"||extension==".glb") {
            AssetTransferRequest request;request.operation=AssetTransferOperation::Reimport;request.source=asset.path;
            request.staging=std::filesystem::temp_directory_path()/("azure-reimport-"+asset.id+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            request.destination=request.staging.parent_path()/(request.staging.filename().u8string()+"-validated");
            auto report=prepareAssetTransfer(request,assetFormatRegistry(),[this]{return cancelled_.load();});
            discardAssetTransfer(report);
        }
        else if(assetTypeRegistry().matches(asset.path,{"image"})) {
            const auto pixels=loadAssetImage(asset.path.u8string());check();
            const auto texture=TextureImportSettings::parse(settings.value("texture",nlohmann::json::object()));
            if(texture.mipPolicy=="generate") {
                static_cast<void>(buildTextureMipChain(pixels.pixels,pixels.width,pixels.height,texture.semantic()));
            }
        }
        else if(assetTypeRegistry().matches(asset.path,{"audio"}))static_cast<void>(validateAudioFile(asset.path,[this]{return cancelled_.load();}));
        check();progress_=1;
        return nlohmann::json{{"asset",asset.id},{"validatedInputs",inputs.size()},{"settings",settings}};
    });
}
AssetReimportJob::~AssetReimportJob(){cancel();if(work_.valid())work_.wait();}
void AssetReimportJob::check() const {if(cancelled_)throw std::runtime_error("Asset reimport cancelled");}
bool AssetReimportJob::ready() const{return work_.valid()&&work_.wait_for(std::chrono::seconds(0))==std::future_status::ready;}
nlohmann::json AssetReimportJob::finish(){auto result=work_.get();check();return result;}
}
