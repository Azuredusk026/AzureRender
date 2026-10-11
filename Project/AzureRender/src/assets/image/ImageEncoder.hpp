#pragma once
#include "assets/AssetTransfer.hpp"
namespace azurerender {
AssetTransferReport prepareImageExport(const AssetTransferRequest& request,const AssetCancellation& cancelled);
AssetTransferReport planImageExport(const AssetTransferRequest& request,const AssetCancellation& cancelled);
nlohmann::json normalizeImageExportSettings(const std::string& format,const nlohmann::json& settings);
}
