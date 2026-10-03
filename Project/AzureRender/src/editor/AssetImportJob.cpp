#include "editor/AssetImportJob.hpp"
#include "assets/GltfLoader.hpp"
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
    progress_=0.65F;checkCancel();(void)loadGltfAsset((staging_/source.filename()).string());checkCancel();progress_=1;
}
bool AssetImportJob::ready()const{return work_.valid()&&work_.wait_for(std::chrono::seconds(0))==std::future_status::ready;}
std::filesystem::path AssetImportJob::finish(){work_.get();checkCancel();std::filesystem::create_directories(destination.parent_path());std::filesystem::rename(staging_,destination);return destination/filename_;}
}
