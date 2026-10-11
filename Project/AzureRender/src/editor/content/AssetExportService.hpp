#pragma once
#include "assets/AssetTransfer.hpp"
#include <map>
namespace azurerender {
class EditorContext;
// Editor owns reviewed plans; generation remains a RenderCore responsibility.
class AssetExportService final {
public:
    nlohmann::json plan(EditorContext&,const nlohmann::json&);
    AssetTransferRequest consume(EditorContext&,const std::string&);
private:
    struct Planned {AssetTransferRequest request;AssetTransferReport report;std::string document;std::uint64_t revision=0,databaseVersion=0;};
    std::map<std::string,Planned> plans_;std::uint64_t sequence_=0;
};
}
