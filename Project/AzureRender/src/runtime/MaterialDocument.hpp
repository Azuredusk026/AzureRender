#pragma once
#include "assets/MaterialData.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/SceneDocument.hpp"
namespace azurerender {
using MaterialDocument=MaterialData;
MaterialBinding resolveMaterialBinding(const std::string& uuid,const AssetDatabase&);
void resolveSceneMaterials(SceneDocument&,const AssetDatabase&);
}
