#pragma once
#include "runtime/SceneDocument.hpp"
#include <functional>
#include <nlohmann/json.hpp>
namespace azurerender {
// Immutable worker-produced proposal. All paths are resolved against the package.
// No database or editor mutation occurs while parsing.
struct SceneImportProposal {
    std::string sourceId;
    std::filesystem::path source;
    SceneDocument scene;
    std::map<std::string,nlohmann::json> components;
    std::map<std::filesystem::path,std::string> sourceFingerprints;
    nlohmann::json diagnostics=nlohmann::json::array();
};
SceneImportProposal readSceneExchange(const std::filesystem::path& path,
    const std::function<bool()>& cancelled={});
}
