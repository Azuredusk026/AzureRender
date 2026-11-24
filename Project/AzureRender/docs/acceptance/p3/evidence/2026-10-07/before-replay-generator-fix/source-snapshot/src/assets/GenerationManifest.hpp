#pragma once
#include <map>
#include <string>
#include <nlohmann/json.hpp>
namespace azurerender {
std::string generationHash(const std::string& bytes);
struct GenerationManifest {
    unsigned schemaVersion=1,generatorVersion=1;
    std::string generatorId,licenseSource,parametersHash;
    std::map<std::string,std::string> inputHashes,dependencyHashes,outputHashes;
    nlohmann::json toJson() const;
    static GenerationManifest fromJson(const nlohmann::json& value);
    std::string fingerprint() const;
};
}
