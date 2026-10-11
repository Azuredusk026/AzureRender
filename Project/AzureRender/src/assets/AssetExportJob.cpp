#include "assets/AssetExportJob.hpp"
#include "assets/image/ImageCodec.hpp"
#include "assets/audio/AudioCodec.hpp"
#include "assets/model/ModelFormatAdapter.hpp"
#include "assets/model/ModelExportScene.hpp"
#include <cmath>
#include <stdexcept>
namespace azurerender {
nlohmann::json normalizeAssetExportSettings(const AssetFormatDescriptor& format,const nlohmann::json& settings){
    if(!format.normalizeExportSettings)throw std::invalid_argument("Export format has no settings contract");
    return format.normalizeExportSettings(settings);
}

AssetTransferReport planAssetExport(const AssetTransferRequest& request,const AssetFormatRegistry& registry,const AssetCancellation& cancel){
    if(cancel&&cancel())throw std::runtime_error("Export planning cancelled");
    const auto& format=registry.require(request.formatId);if(!format.canWrite||request.settingsVersion!=format.settingsVersion||request.maxSourceBytes==0)throw std::invalid_argument("Unsupported export format, version or source budget");
    if(request.scene&&format.domain!=AssetDomain::Model)throw std::invalid_argument("Subtree export requires a model target");
    if(!std::filesystem::is_regular_file(request.source)||std::filesystem::file_size(request.source)>request.maxSourceBytes||registry.detect(request.source).domain!=format.domain)throw std::invalid_argument("Export source is missing, oversized or has a different domain");
    const auto before=assetTransferFingerprint(request.source,cancel);
    auto normalized=request;normalized.settings=normalizeAssetExportSettings(format,request.settings);
    auto report=format.plan(normalized,cancel);
    report.formatId=format.id;report.settingsVersion=request.settingsVersion;report.settings=normalized.settings;
    report.staging=request.staging;report.destination=request.destination;
    if(report.entry.empty()){report.entry=request.source.stem();report.entry+=format.extensions.front();}
    for(const auto& source:report.sourceFingerprints)
        if(assetTransferFingerprint(source.first,cancel)!=source.second)throw std::runtime_error("Export dependency changed during planning");
    if(assetTransferFingerprint(request.source,cancel)!=before)throw std::runtime_error("Export source changed during planning");
    report.sourceFingerprints[request.source]=before;report.summary["losses"]=report.losses;
    report.summary["settings"]=report.settings;report.summary["entry"]=report.entry.generic_u8string();return report;
}
AssetExportJob::AssetExportJob(AssetTransferRequest request,AssetFormatRegistry registry):registry_(std::move(registry)){
    if(request.scene)request.scene=std::make_shared<const ModelExportScene>(*request.scene);
    request.operation=AssetTransferOperation::ExportPlan;plan_=planAssetExport(request,registry_);request.operation=AssetTransferOperation::Export;request.settings=plan_.settings;progress_=.1F;
    work_=std::async(std::launch::async,[this,request=std::move(request)]{const auto cancel=[this]{return cancelled_.load();};
        for(const auto& source:plan_.sourceFingerprints)if(assetTransferFingerprint(source.first,cancel)!=source.second)throw std::runtime_error("Export plan source is stale");
        auto result=prepareAssetTransfer(request,registry_,cancel);
        try{for(const auto& source:plan_.sourceFingerprints)if(assetTransferFingerprint(source.first,cancel)!=source.second)throw std::runtime_error("Export plan source changed during generation");}
        catch(...){discardAssetTransfer(result);throw;}progress_=.9F;return result;});
}

AssetExportJob::~AssetExportJob(){cancel();if(work_.valid())try{auto prepared=work_.get();discardAssetTransfer(prepared);}catch(...){}discardAssetTransfer(report_);}
bool AssetExportJob::ready()const{return work_.valid()&&work_.wait_for(std::chrono::seconds(0))==std::future_status::ready;}
std::filesystem::path AssetExportJob::finish(bool replaceExisting){
    if(finished_||!ready())throw std::logic_error("Export job must be ready and unpublished");report_=work_.get();finished_=true;
    const auto output=publishAssetTransfer(report_,[this]{return cancelled_.load();},replaceExisting);progress_=1;return output;
}
}
