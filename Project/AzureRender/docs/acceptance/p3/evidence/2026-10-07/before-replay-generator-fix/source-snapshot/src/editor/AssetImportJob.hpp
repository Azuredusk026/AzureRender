#pragma once
#include <atomic>
#include <filesystem>
#include <future>
#include <string>
#include <nlohmann/json.hpp>
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
    void copy(const std::filesystem::path& source,const std::filesystem::path& target);
    void checkCancel() const;
    std::filesystem::path staging_;
    std::atomic<bool> cancelled_{false};
    std::atomic<float> progress_{0};
    std::future<void> work_;
    std::string filename_;
    nlohmann::json summary_;
};
}
