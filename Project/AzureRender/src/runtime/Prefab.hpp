#pragma once
#include "runtime/AssetDatabase.hpp"
namespace azurerender {
// Expands immutable prefab source plus node-level merge patches. The original
// document retains prefab references and overrides for editing and saving.
nlohmann::json expandPrefabs(const nlohmann::json& document, const AssetDatabase& assets,const std::map<std::string,nlohmann::json>& sources={});
void remapPrefabReferences(nlohmann::json& node,const std::map<std::string,std::string>& mapping);
nlohmann::json canonicalPrefabNode(nlohmann::json node);
nlohmann::json prefabOverride(const nlohmann::json& base,const nlohmann::json& current);
}
