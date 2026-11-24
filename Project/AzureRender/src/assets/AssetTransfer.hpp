#pragma once
#include "assets/AssetFormatRegistry.hpp"
#include "assets/GenerationManifest.hpp"
#include <cstdint>
#include <nlohmann/json.hpp>
#include <memory>

namespace azurerender {
enum class AssetTransferOperation { Import, Reimport, ExportPlan, Export };
struct ModelExportScene;
struct AssetTransferRequest {
    AssetTransferOperation operation=AssetTransferOperation::Import;
    std::string formatId;
    std::filesystem::path source, staging, destination;
    unsigned settingsVersion=1;
    std::uint64_t maxSourceBytes=512ULL*1024*1024;
    nlohmann::json settings=nlohmann::json::object();
    std::shared_ptr<const ModelExportScene> scene;
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
AssetTransferReport prepareAssetCopy(const AssetTransferRequest& request,
    const AssetCancellation& cancelled);
std::filesystem::path publishAssetTransfer(AssetTransferReport& report,
    const AssetCancellation& cancelled={},bool replaceExisting=false);
void discardAssetTransfer(AssetTransferReport& report) noexcept;
std::string assetTransferFingerprint(const std::filesystem::path& path,
    const AssetCancellation& cancelled={});
}
