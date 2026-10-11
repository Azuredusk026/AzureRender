#include "assets/AssetTransfer.hpp"
#include "assets/GltfLoader.hpp"
#include "assets/model/ModelFormatAdapter.hpp"
#include <array>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace azurerender {
namespace {
void check(const AssetCancellation& cancelled) {
    if(cancelled&&cancelled())throw std::runtime_error("Asset transfer cancelled");
}
bool within(const std::filesystem::path& path,const std::filesystem::path& directory) {
    const auto relative=std::filesystem::weakly_canonical(path).lexically_relative(std::filesystem::weakly_canonical(directory));
    return !relative.empty()&&!relative.is_absolute()&&*relative.begin()!="..";
}
std::string fingerprint(const std::filesystem::path& path,const AssetCancellation& cancelled) {
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Asset transfer input is unavailable");
    std::array<char,65536> bytes{};std::uint64_t hash=14695981039346656037ULL;
    while(input) {check(cancelled);input.read(bytes.data(),bytes.size());for(std::streamsize i=0;i<input.gcount();++i){hash^=static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]);hash*=1099511628211ULL;}}
    if(!input.eof())throw std::runtime_error("Asset transfer input read failed");
    std::ostringstream result;result<<"fnv1a64:"<<std::hex<<std::setfill('0')<<std::setw(16)<<hash;return result.str();
}
void copyInput(const std::filesystem::path& source,const std::filesystem::path& target,
               const AssetTransferRequest& request,AssetTransferReport& report,const AssetCancellation& cancelled) {
    check(cancelled);
    if(!std::filesystem::is_regular_file(source)||std::filesystem::file_size(source)>request.maxSourceBytes)
        throw std::runtime_error("Asset input is missing or exceeds its size budget");
    const auto before=fingerprint(source,cancelled);
    std::filesystem::create_directories(target.parent_path());
    std::ifstream input(source,std::ios::binary);std::ofstream output(target,std::ios::binary);
    std::array<char,65536> bytes{};
    while(input){check(cancelled);input.read(bytes.data(),bytes.size());output.write(bytes.data(),input.gcount());}
    output.flush();if(!input.eof()||!output)throw std::runtime_error("Asset transfer copy failed");output.close();
    if(before!=fingerprint(source,cancelled)||before!=fingerprint(target,cancelled))throw std::runtime_error("Asset source changed during transfer");
    report.sourceFingerprints[source]=before;
    const auto relative=target.lexically_relative(request.staging);
    report.outputs.push_back(relative);report.generation.inputHashes[relative.generic_u8string()]=before;
    report.generation.outputHashes[relative.generic_u8string()]=before;
}
AssetTransferReport prepareGltf(const AssetTransferRequest& request,const AssetCancellation& cancelled) {
    AssetTransferReport report;
    report.entry=request.source.filename();
    copyInput(request.source,request.staging/report.entry,request,report,cancelled);
    if(assetExtension(request.source)==".gltf") {
        std::ifstream input(request.staging/report.entry);nlohmann::json document;input>>document;
        for(const char* key:{"buffers","images"})for(const auto& value:document.value(key,nlohmann::json::array())) {
            const auto uri=value.value("uri",std::string());if(uri.empty()||uri.rfind("data:",0)==0)continue;
            const auto relative=std::filesystem::u8path(uri).lexically_normal();
            if(relative.empty()||relative.is_absolute()||relative.has_root_name()||*relative.begin()==".."||
               !within(request.source.parent_path()/relative,request.source.parent_path()))
                throw std::invalid_argument("Asset dependency must remain inside its source directory");
            if(relative==report.entry)throw std::invalid_argument("Asset dependency cannot overwrite its container");
            const auto dependency=request.source.parent_path()/relative;
            if(report.sourceFingerprints.count(dependency))continue;
            copyInput(dependency,request.staging/relative,request,report,cancelled);
            report.dependencies.push_back(dependency);
            report.generation.dependencyHashes[relative.generic_u8string()]=report.sourceFingerprints.at(dependency);
        }
    }
    check(cancelled);const auto asset=loadGltfAsset((request.staging/report.entry).u8string());
    report.summary={{"source",request.source.u8string()},{"vertices",asset.vertices.size()},{"indices",asset.indices.size()},
        {"materials",asset.materials.size()},{"joints",asset.jointNodes.size()},{"skinned",asset.hasSkin},{"clips",nlohmann::json::array()}};
    for(std::size_t i=0;i<asset.animations.size();++i){const auto& clip=asset.animations[i];report.summary["clips"].push_back({{"index",i},{"name",clip.name},{"duration",clip.endTime-clip.startTime},{"channels",clip.channels.size()}});}
    return report;
}
}
const AssetFormatRegistry& assetFormatRegistry() {
    static const auto registry=[] {
        AssetFormatRegistry result;
        for(const std::string id:{"gltf","glb"}) {
            AssetFormatDescriptor format;format.id=id;format.label=id=="gltf"?"glTF 2.0":"GLB 2.0";
            format.extensions={"."+id};format.features={"geometry","materials","skin","animation"};format.canRead=true;
            format.probe=[id](const std::vector<unsigned char>& bytes) {
                if(id=="glb")return bytes.size()>=12&&bytes[0]=='g'&&bytes[1]=='l'&&bytes[2]=='T'&&bytes[3]=='F'&&bytes[4]==2&&bytes[5]==0&&bytes[6]==0&&bytes[7]==0;
                // Bounded prefix probing selects a candidate; the production parser validates the complete document.
                const std::string prefix(bytes.begin(),bytes.end());
                return prefix.find('{')!=std::string::npos&&prefix.find("\"asset\"")!=std::string::npos&&prefix.find("\"version\"")!=std::string::npos;
            };
            format.prepare=prepareGltf;result.add(std::move(format));
        }
        registerModelFormats(result);return result;
    }();return registry;
}
AssetTransferReport prepareAssetTransfer(const AssetTransferRequest& request,const AssetFormatRegistry& registry,const AssetCancellation& cancelled) {
    check(cancelled);
    const auto& format=request.formatId.empty()?registry.detect(request.source):registry.require(request.formatId);
    if(request.settingsVersion!=format.settingsVersion||!request.settings.is_object()||request.maxSourceBytes==0)
        throw std::invalid_argument("Unsupported asset transfer settings or budget");
    if(request.operation==AssetTransferOperation::Export||request.operation==AssetTransferOperation::ExportPlan) {
        if(!format.canWrite)throw std::invalid_argument("Asset format does not support export");
    } else {
        if(!format.canRead||registry.detect(request.source).id!=format.id)throw std::invalid_argument("Asset content differs from the requested format");
    }
    if(request.staging.empty()||request.destination.empty()||request.staging==request.destination||
       within(request.source,request.staging)||within(request.source,request.destination)||
       within(request.staging,request.destination)||within(request.destination,request.staging))
        throw std::invalid_argument("Asset source, staging and destination must have separate ownership");
    if(std::filesystem::exists(request.staging))throw std::invalid_argument("Asset staging directory is occupied");
    // The caller supplies a unique task directory. Preparation owns only the directory it creates.
    if(!std::filesystem::create_directories(request.staging))throw std::runtime_error("Cannot acquire asset staging directory");
    try {
        auto report=format.prepare(request,cancelled);check(cancelled);
        report.formatId=format.id;report.settingsVersion=request.settingsVersion;report.settings=request.settings;
        report.staging=request.staging;report.destination=request.destination;
        report.generation.generatorId="azure."+format.id+"-import";report.generation.generatorVersion=format.generatorVersion;
        report.generation.licenseSource="LicenseRef-External-Asset";
        report.generation.parametersHash=generationHash(nlohmann::json{{"filename",request.source.filename().generic_u8string()},{"settings",request.settings},{"settingsVersion",request.settingsVersion},{"generatorVersion",format.generatorVersion}}.dump());
        if(report.outputs.empty()||!within(report.staging/report.entry,report.staging))throw std::runtime_error("Asset adapter produced an invalid entry");
        for(const auto& path:report.outputs)if(path.is_absolute()||!within(report.staging/path,report.staging)||!std::filesystem::is_regular_file(report.staging/path))throw std::runtime_error("Asset adapter produced an invalid output");
        report.summary["generation"]=GenerationManifest::fromJson(report.generation.toJson()).toJson();return report;
    } catch(...) {std::error_code error;std::filesystem::remove_all(request.staging,error);throw;}
}
void discardAssetTransfer(AssetTransferReport& report) noexcept {
    if(report.staging.empty())return;
    std::error_code error;std::filesystem::remove_all(report.staging,error);report.staging.clear();
}
std::filesystem::path publishAssetTransfer(AssetTransferReport& report,const AssetCancellation& cancelled) {
    try {
        check(cancelled);
        if(report.staging.empty()||std::filesystem::exists(report.destination))throw std::runtime_error("Asset destination is occupied or staging is unavailable");
        for(const auto& pair:report.sourceFingerprints)if(fingerprint(pair.first,cancelled)!=pair.second)throw std::runtime_error("Asset source changed before publication");
        for(const auto& path:report.outputs) {
            const auto expected=report.generation.outputHashes.find(path.generic_u8string());
            if(expected==report.generation.outputHashes.end()||fingerprint(report.staging/path,cancelled)!=expected->second)throw std::runtime_error("Asset output changed before publication");
        }
        check(cancelled);std::filesystem::create_directories(report.destination.parent_path());
        std::filesystem::rename(report.staging,report.destination);report.staging.clear();return report.destination/report.entry;
    } catch(...) {discardAssetTransfer(report);throw;}
}
}
