#pragma once
#include "assets/generators/ProceduralContracts.hpp"
namespace azurerender {
struct RigTextRequest {
    ProceduralGeometryRequest geometry;
    double scale=1;
};
struct RigTextResult {
    std::string gltf;
    nlohmann::json animationGraph;
};
RigTextResult generateRigText(const RigTextRequest&);
}
