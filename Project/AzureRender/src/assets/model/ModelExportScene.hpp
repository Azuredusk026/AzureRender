#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <vector>
namespace azurerender {
struct ModelExportNode {
    std::string id,parent;
    std::filesystem::path source;
    // Column-major local affine transform. Empty source is a group node.
    std::array<float,16> transform{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
};
struct ModelExportScene {std::vector<ModelExportNode> nodes;};
}
