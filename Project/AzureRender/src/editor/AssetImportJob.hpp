#pragma once
#include <atomic>
#include <filesystem>
#include <future>
#include <string>
#include <nlohmann/json.hpp>
#include "runtime/AssetDatabase.hpp"
#include "assets/AssetTransfer.hpp"
namespace azurerender {
class AssetImportJob {
public:
    AssetImportJob(std::filesystem::path source,std::filesystem::path staging,std::filesystem::path destination);
    ~AssetImportJob();
    void cancel() { cancelled_=true; }
    bool ready() const;
    float progress() const { return progress_.load(); }
    std::filesystem::path finish();
    const nlohmann::json& summary() const noexcept { return summary_; }
    const std::filesystem::path destination;
private:
    void prepare(const std::filesystem::path& source);
    void checkCancel() const;
    std::filesystem::path staging_;
    std::atomic<bool> cancelled_{false};
    std::atomic<float> progress_{0};
    std::future<void> work_;
    std::string filename_;
    nlohmann::json summary_;
    AssetTransferReport transfer_;
};
// Validates existing project content without publishing from a worker thread.
class AssetReimportJob final {
public:
    AssetReimportJob(AssetRecord asset,std::vector<AssetRecord> inputs,nlohmann::json settings);
    ~AssetReimportJob();
    void cancel() {cancelled_=true;}
    bool ready() const;
    float progress() const {return progress_.load();}
    nlohmann::json finish();
private:
    std::atomic<bool> cancelled_{false};
    std::atomic<float> progress_{0};
    std::future<nlohmann::json> work_;
    void check() const;
};
}
