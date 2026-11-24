#pragma once
#include "assets/AssetTransfer.hpp"
#include <atomic>
#include <future>
namespace azurerender {
nlohmann::json normalizeAssetExportSettings(const AssetFormatDescriptor& format,const nlohmann::json& settings);
AssetTransferReport planAssetExport(const AssetTransferRequest& request,const AssetFormatRegistry& registry,
    const AssetCancellation& cancelled={});
// RenderCore owns preparation. The caller owns this job and publishes on its
// controlling thread. Destruction cancels, joins and discards owned staging.
class AssetExportJob final {
public:
    explicit AssetExportJob(AssetTransferRequest request,AssetFormatRegistry registry=assetFormatRegistry());
    ~AssetExportJob();
    void cancel() noexcept{cancelled_=true;}
    bool ready() const;
    float progress() const noexcept{return progress_.load();}
    const AssetTransferReport& plan() const noexcept{return plan_;}
    const AssetTransferReport& report() const noexcept{return report_;}
    std::filesystem::path finish(bool replaceExisting=false);
private:
    std::atomic<bool> cancelled_{false};std::atomic<float> progress_{0};
    AssetFormatRegistry registry_;
    AssetTransferReport plan_,report_;std::future<AssetTransferReport> work_;bool finished_=false;
};
}
