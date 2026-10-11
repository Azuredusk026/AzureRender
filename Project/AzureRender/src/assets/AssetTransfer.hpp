#pragma once
#include "assets/AssetFormatRegistry.hpp"
#include "assets/GenerationManifest.hpp"
#include <cstdint>
#include <nlohmann/json.hpp>

namespace azurerender {
enum class AssetTransferOperation { Import, Reimport, ExportPlan, Export };
struct AssetTransferRequest {
    AssetTransferOperation operation=AssetTransferOperation::Import;
    std::string formatId;
    std::filesystem::path source, staging, destination;
    unsigned settingsVersion=1;
    std::uint64_t maxSourceBytes=512ULL*1024*1024;
    nlohmann::json settings=nlohmann::json::object();
};
struct AssetTransferReport {
    std::string formatId;
    unsigned settingsVersion=1;
    std::filesystem::path staging, destination, entry;
    std::vector<std::filesystem::path> outputs, dependencies;
    std::vector<std::string> warnings, losses;
    std::map<std::filesystem::path,std::string> sourceFingerprints;
    GenerationManifest generation;
    nlohmann::json settings=nlohmann::json::object();
    nlohmann::json summary=nlohmann::json::object();
};
AssetTransferReport prepareAssetTransfer(const AssetTransferRequest& request,
    const AssetFormatRegistry& registry,const AssetCancellation& cancelled);
std::filesystem::path publishAssetTransfer(AssetTransferReport& report,
    const AssetCancellation& cancelled={});
void discardAssetTransfer(AssetTransferReport& report) noexcept;
}
