#pragma once
#include "runtime/AssetDatabase.hpp"
namespace azurerender {
// Expands immutable prefab source plus node-level merge patches. The original
// document retains prefab references and overrides for editing and saving.
nlohmann::json expandPrefabs(const nlohmann::json& document, const AssetDatabase& assets);
}
