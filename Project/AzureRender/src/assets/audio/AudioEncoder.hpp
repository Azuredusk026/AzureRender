#pragma once
#include "assets/AssetTransfer.hpp"
namespace azurerender {
AssetTransferReport prepareAudioExport(const AssetTransferRequest& request,const AssetCancellation& cancelled);
AssetTransferReport planAudioExport(const AssetTransferRequest& request,const AssetCancellation& cancelled);
nlohmann::json normalizeAudioExportSettings(const std::string& format,const nlohmann::json& settings);
}
