#pragma once
#include "assets/AssetTransfer.hpp"
namespace azurerender {
// RenderCore owns conversion; each call owns its importer and temporary scene.
void registerModelFormats(AssetFormatRegistry& registry);
AssetTransferReport prepareModelExport(const AssetTransferRequest& request,const AssetCancellation& cancelled);
AssetTransferReport inspectModelExport(const AssetTransferRequest& request,const AssetCancellation& cancelled);
nlohmann::json normalizeModelExportSettings(const nlohmann::json& settings);
}
