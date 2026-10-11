#pragma once
#include "assets/AssetTransfer.hpp"
namespace azurerender {
// RenderCore owns conversion; each call owns its importer and temporary scene.
void registerModelFormats(AssetFormatRegistry& registry);
}
